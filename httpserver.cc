#include "httpserver.h"
#include "log.h"
#include <unistd.h>



HttpServer::HttpServer()
        : ServerBase() {
}

HttpServer::~HttpServer() = default;

HttpServer::HttpNetThread::HttpNetThread()
        : NetThreadBase() {
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
        return HandleHttpRequest(_conn);
    }
    return 0;
}

int HttpServer::HttpNetThread::_OnWriteEvent(tcp::SendContext *_send_ctx,
                                             bool _del) {
    if (!_send_ctx) {
        LogE("!_send_ctx")
        return -1;
    }
    AutoBuffer &resp = _send_ctx->buffer;
    size_t pos = resp.Pos();
    size_t ntotal = resp.Length() - pos;
    SOCKET fd = _send_ctx->fd;
    
    if (fd < 0 || ntotal == 0) {
        return 0;
    }
    
    ssize_t nsend = ::write(fd, resp.Ptr(pos), ntotal);
    
    do {
        if (nsend == ntotal) {
            LogI("fd(%d), send %zd/%zu B, done", fd, nsend, ntotal)
            resp.Seek(resp.Length());
            break;
        }
        if (nsend >= 0 || (nsend < 0 && IS_EAGAIN(errno))) {
            nsend = nsend > 0 ? nsend : 0;
            LogI("fd(%d): send %zd/%zu B", fd, nsend, ntotal)
            resp.Seek(pos + nsend);
            return 0;
        }
        if (nsend < 0) {
            if (errno == EPIPE) {
                // fd probably closed by peer, or cleared because of timeout.
                LogI("fd(%d) already closed, send nothing", fd)
                return 0;
            }
            LogE("fd(%d) nsend(%zd), errno(%d): %s",
                 fd, nsend, errno, strerror(errno));
        }
    } while (false);
    
    if (_del) {
        DelConnection(_send_ctx->connection_uid);
    }
    
    return nsend < 0 ? -1 : 0;
}


HttpServer::HttpNetThread::~HttpNetThread() = default;
