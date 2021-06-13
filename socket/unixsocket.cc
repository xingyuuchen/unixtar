#include <unistd.h>
#include "unixsocket.h"
#include "cassert"
#include <fcntl.h>
#include <cerrno>
#include "log.h"


const int Socket::kBufferSize = 4096;

Socket::Socket(SOCKET _fd, int _type /* = SOCK_STREAM*/,
               bool _nonblocking /* = true*/, bool _connected /* = false*/)
        : fd_(_fd)
        , type_(_type)
        , errno_(0)
        , is_eagain_(false)
        , is_connected_(_connected)
        , nonblocking_(_nonblocking) {
    
    assert(_type == SOCK_STREAM || _type == SOCK_DGRAM);
    
    if (fd_ > 0 && _nonblocking) {
        SetNonblocking();
    }
}

Socket::~Socket() {
    Close();
}

int Socket::Create(int _domain, int _type, int _protocol /* = 0*/) {
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

int Socket::Bind(sa_family_t _sin_family, uint16_t _port,
                 in_addr_t _in_addr/* = INADDR_ANY*/) const {
    
    assert(_sin_family == AF_INET || _sin_family == AF_INET6);
    
    struct sockaddr_in sock_addr{};
    memset(&sock_addr, 0, sizeof(sock_addr));
    
    sock_addr.sin_family = _sin_family;
    sock_addr.sin_addr.s_addr = htonl(_in_addr);
    sock_addr.sin_port = htons(_port);
    
    int ret = ::bind(fd_, (struct sockaddr *) &sock_addr,
                     sizeof(sock_addr));
    if (ret < 0) {
        LogE("bind errno(%d): %s", errno, strerror(errno));
        return -1;
    }
    return 0;
}

int Socket::Connect(std::string &_ip, uint16_t _port) {
    struct sockaddr_in sockaddr{};
    memset(&sockaddr, 0, sizeof(sockaddr));
    
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons(_port);
    sockaddr.sin_addr.s_addr = inet_addr(_ip.c_str());
    
    int ret = connect(fd_, (struct sockaddr *) &sockaddr,
            sizeof(sockaddr));
    if (ret < 0) {
        is_connected_ = false;
        LogE("connect errno(%d): %s", errno, strerror(errno))
    } else {
        is_connected_ = true;
    }
    return ret;
}

void Socket::SetConnected(bool _connected) { is_connected_ = _connected; }

int Socket::SetNonblocking() {
    if (fd_ < 0) {
        LogE("invalid socket")
        return -1;
    }
    int old_flags = ::fcntl(fd_, F_GETFL);
    if (::fcntl(fd_, F_SETFL, old_flags | O_NONBLOCK) == -1) {
        LogE("fcntl errno(%d): %s", errno, strerror(errno))
        return -1;
    }
    nonblocking_ = true;
    return 0;
}

int Socket::SetTcpNoDelay() const {
    int tcp_no_delay = 1;
    return SetSocketOpt(IPPROTO_TCP, TCP_NODELAY,
                &tcp_no_delay, sizeof(tcp_no_delay));;
}

int Socket::SetTcpKeepAlive() const {
    int keep_alive = 1;
    return SetSocketOpt(SOL_SOCKET, SO_KEEPALIVE,
                        &keep_alive, sizeof(keep_alive));
}

int Socket::SetCloseLingerTimeout(int _linger) const {
    assert(_linger >= 0);
    struct linger ling{};
    ling.l_onoff = 1;
    ling.l_linger = _linger;
    return SetSocketOpt(SOL_SOCKET, SO_LINGER, &ling, sizeof(ling));
}

int Socket::Listen(int _backlog) const {
    assert(_backlog > 0);
    int ret = ::listen(fd_, _backlog);
    if (ret < 0) {
        LogE("listen errno(%d): %s", errno, strerror(errno))
    }
    return ret;
}

ssize_t Socket::Receive(AutoBuffer *_buff, bool *_is_buffer_full) {
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
    assert(fd_ < 0);
    if (_fd > 0) {
        fd_ = _fd;
    }
}

int Socket::ShutDown(int _how) const {
    if (_how != SHUT_RD && _how != SHUT_WR && _how != SHUT_RDWR) {
        LogE("invalid how: %d", _how)
        assert(false);
    }
    if (is_connected_) {
        int ret = shutdown(fd_, _how);
        if (ret < 0) {
            LogE("ret: %d, errno(%d): %s", ret, errno, strerror(errno))
        }
        return ret;
    }
    return 0;
}

void Socket::Close() {
    if (fd_ > 0 && IsTcp()) {
        LogD("fd(%d)", fd_)
        /**
         * A standard TCP connection gets terminated by 4-way finalization:
         *   Once a participant has no more data to send, it sends a FIN packet to the other.
         *   The other party returns an ACK for the FIN.
         *   When the other party also finished data transfer, it sends another FIN packet.
         *   The initial participant returns an ACK and finalizes transfer.
         */
        int ret = ::close(fd_);
        if (ret < 0) {
            LogE("ret: %d, errno(%d): %s", ret, errno, strerror(errno))
        }
        fd_ = INVALID_SOCKET;
    }
}

SOCKET Socket::FD() const {
    return fd_;
}

bool Socket::IsNonblocking() const {
    return nonblocking_;
}

int Socket::SocketError() const {
    int error = 0;
    socklen_t errlen = sizeof(error);
    if (getsockopt(fd_, SOL_SOCKET, SO_ERROR, (void *) &error, &errlen) == 0) {
        LogE("error = %s\n", strerror(error))
    }
    return error;
}

int Socket::SetSocketOpt(int _level, int _option_name,
                         const void *_option_value,
                         socklen_t _option_len) const {
    assert(fd_ > 0);
    int ret = ::setsockopt(fd_, _level, _option_name,
                           _option_value, _option_len);
    if (ret < 0) {
        LogE("ret: %d, errno(%d): %s", ret, errno, strerror(errno))
    }
    return ret;
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

