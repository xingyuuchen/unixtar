#include "httpserver.h"
#include "log.h"
#include <unistd.h>
#include "httprequest.h"
#include "httpresponse.h"



HttpServer::HttpServer()
        : ServerBase() {
}

HttpServer::~HttpServer() = default;

HttpServer::HttpNetThread::HttpNetThread()
        : NetThreadBase() {
}

void HttpServer::HttpNetThread::ConfigApplicationLayer(tcp::ConnectionProfile *_conn) {
    if (_conn->GetType() == tcp::ConnectionProfile::kFrom) {
        _conn->ConfigApplicationLayer<http::request::HttpRequest, http::request::Parser>();
    } else {
        _conn->ConfigApplicationLayer<http::response::HttpResponse, http::response::Parser>();
    }
}

int HttpServer::HttpNetThread::_OnReadEvent(tcp::ConnectionProfile *_conn) {
    assert(_conn);
    
    SOCKET fd = _conn->FD();
    uint32_t uid = _conn->Uid();
    LogI("fd(%d), uid: %u", fd, uid)
    
    if (_conn->Receive() < 0) {
        DelConnection(uid);
        return -1;
    }
    
    if (_conn->IsParseDone()) {
        LogI("fd(%d) http parse succeed", fd)
        return HandleHttpPacket(_conn);
    }
    return 0;
}

bool HttpServer::HttpNetThread::_OnWriteEvent(tcp::SendContext *_send_ctx) {
    if (!_send_ctx) {
        LogE("!_send_ctx")
        return false;
    }
    AutoBuffer &resp = _send_ctx->buffer;
    size_t pos = resp.Pos();
    size_t ntotal = resp.Length() - pos;
    SOCKET fd = _send_ctx->fd;
    
    if (fd <= 0 || ntotal == 0) {
        return false;
    }
    
    ssize_t nsend = ::write(fd, resp.Ptr(pos), ntotal);
    
    if (nsend == ntotal) {
        LogI("fd(%d), send %zd/%zu B, done", fd, nsend, ntotal)
        resp.Seek(AutoBuffer::kEnd);
        return true;
    }
    if (nsend >= 0 || (nsend < 0 && IS_EAGAIN(errno))) {
        nsend = nsend > 0 ? nsend : 0;
        LogI("fd(%d): send %zd/%zu B", fd, nsend, ntotal)
        resp.Seek(AutoBuffer::kCurrent, nsend);
    }
    if (nsend < 0) {
        if (errno == EPIPE) {
            // fd probably closed by peer, or cleared because of timeout.
            LogI("fd(%d) already closed, send nothing", fd)
            return false;
        }
        LogE("fd(%d) nsend(%zd), errno(%d): %s",
             fd, nsend, errno, strerror(errno))
        LogPrintStacktrace(5)
    }
    return false;
}


HttpServer::HttpNetThread::~HttpNetThread() = default;
