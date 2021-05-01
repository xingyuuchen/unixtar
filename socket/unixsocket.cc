#include <unistd.h>
#include "unixsocket.h"
#include "cassert"
#include <fcntl.h>
#include <cerrno>
#include "log.h"


const int Socket::kBufferSize = 1024;

Socket::Socket(SOCKET _fd, int _type/* = SOCK_STREAM*/, bool _nonblocking/* = true*/)
        : fd_(_fd)
        , type_(_type)
        , errno_(0)
        , is_eagain_(false)
        , nonblocking_(_nonblocking) {
    assert(_type == SOCK_STREAM || _type == SOCK_DGRAM);
    
    if (fd_ > 0 && _nonblocking) {
        SetNonblocking();
    }
}

Socket::~Socket() {
    Close();
}

int Socket::Create(int _domain, int _type, int _protocol) {
    assert(_type == SOCK_STREAM || _type == SOCK_DGRAM);
    type_ = _type;
    
    fd_ = ::socket(_domain, _type, _protocol);
    if (fd_ < 0) {
        LogE("create socket error: %s, errno: %d",
             strerror(errno), errno);
        return -1;
    }
    return 0;
}

int Socket::SetNonblocking() {
    int old_flags = ::fcntl(fd_, F_GETFL);
    if (::fcntl(fd_, F_SETFL, old_flags | O_NONBLOCK) == -1) {
        return -1;
    }
    nonblocking_ = true;
    return 0;
}

ssize_t Socket::Recv(AutoBuffer *_buff, bool *_is_buffer_full) {
    assert(_buff && _is_buffer_full);
    
    size_t available = _buff->AvailableSize();
    if (available < kBufferSize) {
        _buff->AddCapacity(kBufferSize - available);
    }
    
    ssize_t n = ::read(fd_, _buff->Ptr(
            _buff->Length()), kBufferSize);
    
    if (n > 0) {
        _buff->AddLength(n);
        
    } else if (n < 0) {
        errno_ = errno;
        is_eagain_ = n == -1 && IS_EAGAIN(errno_);
        if (!is_eagain_) {
            LogE("n(%zd), errno(%d): %s", n, errno, strerror(errno))
        }
    }
    
    *_is_buffer_full = n == kBufferSize;
    
    return n;
}

void Socket::Set(SOCKET _fd) {
    fd_ = _fd;
}

void Socket::Close() {
    if (fd_ > 0 && IsTcp()) {
        LogI("fd(%d)", fd_)
        /**
         * A standard TCP connection gets terminated by 4-way finalization:
         *   Once a participant has no more data to send, it sends a FIN packet to the other
         *   The other party returns an ACK for the FIN.
         *   When the other party also finished data transfer, it sends another FIN packet
         *   The initial participant returns an ACK and finalizes transfer.
         */
        ::close(fd_), fd_ = INVALID_SOCKET;
    }
}

SOCKET Socket::FD() const {
    return fd_;
}

bool Socket::IsNonblocking() const {
    return nonblocking_;
}

int Socket::SetSocketOpt(int _level, int _option_name,
                         const void *_option_value,
                         socklen_t _option_len) const {
    if (fd_ < 0) {
        LogE("fd_(%d) < 0", fd_)
        assert(false);
    }
    return ::setsockopt(fd_, _level, _option_name, _option_value, _option_len);
}

bool Socket::IsTcp() const {
    return type_ == SOCK_STREAM;
}

bool Socket::IsUdp() const {
    return type_ == SOCK_DGRAM;
}

bool Socket::IsEAgain() const {
    return is_eagain_;
}

