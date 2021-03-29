#include "connectionprofile.h"
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "timeutil.h"
#include "log.h"
#include "socket/unix_socket.h"


namespace Tcp {

const int ConnectionProfile::kBuffSize = 1024;

const uint64_t ConnectionProfile::kDefaultTimeout = 30 * 1000;

ConnectionProfile::ConnectionProfile(int _fd)
        : fd_(_fd)
        , application_protocol_(TApplicationProtocol::kHttp1_1)
        , record_(::gettickcount())
        , timeout_millis_(kDefaultTimeout)
        , timeout_ts_(record_ + timeout_millis_) {
    
    if (_fd < 0) {
        fd_ = INVALID_SOCKET;
    }
    
}

int ConnectionProfile::Receive() {
    AutoBuffer *recv_buff = http_parser_.GetBuff();
    
    while (true) {
    
        size_t available = recv_buff->AvailableSize();
        if (available < kBuffSize) {
            recv_buff->AddCapacity(kBuffSize - available);
        }
        
        ssize_t n = ::read(fd_, recv_buff->Ptr(
                recv_buff->Length()), kBuffSize);
        
        if (n == -1 && IS_EAGAIN(errno)) {
            LogI(__FILE__, "[Receive] EAGAIN")
            return 0;
        }
        if (n < 0) {
            LogE(__FILE__, "[Receive] n<0, errno(%d): %s", errno, strerror(errno))
            return -1;
            
        } else if (n == 0) {
            // A read event is raised when conn closed by peer
            LogI(__FILE__, "[Receive] conn(%d) closed by peer", fd_)
            return -1;
            
        } else if (n > 0) {
            recv_buff->AddLength(n);
        }
    
        int ret = ParseProtocol();
        
        if (ret == -1) {
            LogE(__FILE__, "[Receive] Please override func: ParseProtocol "
                           "when using other protocol than Http1.1")
            return -1;
        } else if (ret == -2) {
            return -1;
        }
    
        if (IsParseDone()) {
            __MakeRecvContext();
            return 0;
        }
    
        if (n < kBuffSize) {
            return 0;
        }
    }
}


int ConnectionProfile::ParseProtocol() {
    if (application_protocol_ != TApplicationProtocol::kHttp1_1) {
        // Override me if you use other application protocol
        // other than Http1_1. :)
        return -1;
    }
    
    http_parser_.DoParse();
    
    if (http_parser_.IsErr()) {
        LogE(__FILE__, "[__HandleRead] parser error")
        return -2;
    }
    
    return 0;
}


bool ConnectionProfile::IsParseDone() {
    if (application_protocol_ != TApplicationProtocol::kHttp1_1) {
        return false;
    }
    return http_parser_.IsEnd();
}

void ConnectionProfile::CloseSelf() {
    if (fd_ > 0) {
        LogI(__FILE__, "[CloseSelf] %d", fd_)
        ::shutdown(fd_, SHUT_RDWR);
        fd_ = INVALID_SOCKET;
    }
}

ConnectionProfile::TApplicationProtocol ConnectionProfile::GetApplicationProtocol() const {
    return application_protocol_;
}


http::request::Parser *ConnectionProfile::GetHttpParser() { return &http_parser_; }

SOCKET ConnectionProfile::FD() const { return fd_; }

RecvContext *ConnectionProfile::GetRecvContext() { return &recv_ctx_; }

SendContext *ConnectionProfile::GetSendContext() { return &send_ctx_; }

uint64_t ConnectionProfile::GetTimeoutTs() const { return timeout_ts_; }

bool ConnectionProfile::IsTimeout(uint64_t _now) const {
    if (_now == 0) {
        _now = ::gettickcount();
    }
    return _now > timeout_ts_;
}

void ConnectionProfile::__MakeRecvContext() {
    if (fd_ < 0) {
        return;
    }
    if (!IsParseDone()) {
        LogE(__FILE__, "[__MakeRecvContext] recv ctx cannot be made "
                       "when the parsing hasn't done")
        return;
    }
    recv_ctx_.fd = fd_;
    recv_ctx_.buffer.SetPtr(http_parser_.GetBody());
    recv_ctx_.buffer.SetLength(http_parser_.GetContentLength());
    recv_ctx_.buffer.ShareFromOther(true);
    
    send_ctx_.fd = fd_;
    recv_ctx_.send_context = &send_ctx_;
}

ConnectionProfile::~ConnectionProfile() {
    CloseSelf();
}

}
