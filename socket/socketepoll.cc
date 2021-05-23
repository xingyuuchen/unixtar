#include "socketepoll.h"
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <cassert>
#include "log.h"


const int SocketEpoll::kMaxFds = 1024;

SocketEpoll::SocketEpoll(int _max_fds)
        : epoll_fd_(INVALID_SOCKET)
        , listen_fd_(INVALID_SOCKET)
        , epoll_events_(nullptr)
        , errno_(0)
{
#ifdef __linux__
    if (_max_fds <= 0) { return; }
    
    epoll_events_ = new struct epoll_event[_max_fds];
    
    int ret = ::epoll_create1(0);
    if (ret < 0) {
        LogE("epoll_create, ret = %d", ret)
        return;
    }
    epoll_fd_ = ret;
#endif
}


int SocketEpoll::AddSocketRead(int _fd) {
    return AddSocketRead(_fd, _fd);
}

int SocketEpoll::AddSocketRead(int _fd, uint64_t _data) {
#ifdef __linux__
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = _fd;
    return __EpollCtl(EPOLL_CTL_ADD, _fd, &event);
#else
    return 0;
#endif
}

int SocketEpoll::AddSocketReadWrite(int _fd, uint64_t _data) {
#ifdef __linux__
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLOUT | EPOLLET;
    event.data.u64 = _data;
    return __EpollCtl(EPOLL_CTL_ADD, _fd, &event);
#else
    return 0;
#endif
}

int SocketEpoll::ModSocketWrite(int _fd, uint64_t _data) {
#ifdef __linux__
    struct epoll_event event;
    event.events = EPOLLOUT | EPOLLET;
    event.data.u64 = _data;
    return __EpollCtl(EPOLL_CTL_MOD, _fd, &event);
#else
    return 0;
#endif
}

int SocketEpoll::DelSocket(int _fd) {
#ifdef __linux__
    return __EpollCtl(EPOLL_CTL_DEL, _fd);
#else
    return 0;
#endif
}

int SocketEpoll::__EpollCtl(int _op, int _fd, struct epoll_event *_event/* = nullptr*/) {
#ifdef __linux__
    if (_fd < 0) {
        LogE("fd_ < 0")
        return -1;
    }
    assert(epoll_fd_ > 0);
    
    int ret = ::epoll_ctl(epoll_fd_, _op, _fd, _event);
    if (ret < 0) {
        errno_ = errno;
        LogE("fd(%d), errno(%d): %s", _fd, errno_, strerror(errno_))
    }
    return ret;
#else
    return 0;
#endif
}


int SocketEpoll::EpollWait(int _timeout_mills/* = -1*/,
                           int _max_events/* = kMaxFds*/) {
#ifdef __linux__
    if (_timeout_mills < -1) { _timeout_mills = -1; }
    assert(epoll_fd_ > 0);
    
    int retry = 10;
    while (--retry) {
        int nfds = ::epoll_wait(epoll_fd_, epoll_events_, _max_events, _timeout_mills);
        if (nfds < 0) {
            if (errno == EINTR) {
                LogE("just EINTR, continue...")
                continue;
            }
            errno_ = errno;
            LogE("errno(%d): %s", errno_, strerror(errno))
        }
        return nfds;
    }
    return -1;
#else
    return 0;
#endif
}

SOCKET SocketEpoll::GetSocket(int _idx) {
#ifdef __linux__
    if (_idx < 0 || _idx >= kMaxFds) {
        LogE("invalid _idx: %d", _idx)
        return INVALID_SOCKET;
    }
    return epoll_events_[_idx].data.fd;
#endif
    return INVALID_SOCKET;
}

void *SocketEpoll::GetEpollDataPtr(int _idx) {
#ifdef __linux__
    if (_idx < 0 || _idx >= kMaxFds) {
        LogE("invalid _idx: %d", _idx)
        return nullptr;
    }
    return epoll_events_[_idx].data.ptr;
#endif
    return nullptr;
}

uint64_t SocketEpoll::IsReadSet(int _idx) {
#ifdef __linux__
    epoll_data *ret = __IsFlagSet(_idx, EPOLLIN);
    return ret == nullptr ? 0 : ret->u64;
#else
    return 0;
#endif
}

uint64_t SocketEpoll::IsWriteSet(int _idx) {
#ifdef __linux__
    epoll_data *ret = __IsFlagSet(_idx, EPOLLOUT);
    return ret == nullptr ? 0 : ret->u64;
#else
    return 0;
#endif
}

bool SocketEpoll::IsNewConnect(int _idx) {
#ifdef __linux__
    if (_idx < 0 || _idx >= kMaxFds) {
        LogE("invalid _idx: %d", _idx)
        return false;
    }
    return epoll_events_[_idx].data.fd == listen_fd_;
#else
    return false;
#endif
}

uint64_t SocketEpoll::IsErrSet(int _idx) {
#ifdef __linux__
    epoll_data *ret = __IsFlagSet(_idx, EPOLLERR);
    return ret == nullptr ? 0 : ret->u64;
#else
    return 0;
#endif
}

epoll_data *SocketEpoll::__IsFlagSet(int _idx, int _flag) {
#ifdef __linux__
    if (_idx < 0 || _idx >= kMaxFds) {
        LogE("invalid _idx: %d", _idx)
        return nullptr;
    }
    if (epoll_events_[_idx].events & _flag) {
        return &epoll_events_[_idx].data;
    }
#endif
    return nullptr;
}


void SocketEpoll::SetListenFd(int _listen_fd) {
#ifdef __linux__
    if (_listen_fd < 0) {
        LogE("_listen_fd: %d", _listen_fd)
        return;
    }
    AddSocketRead(_listen_fd);
    listen_fd_ = _listen_fd;
#endif
}

int SocketEpoll::GetErrNo() const { return errno_; }

SocketEpoll::~SocketEpoll() {
#ifdef __linux__
    if (epoll_events_) {
        delete[] epoll_events_, epoll_events_ = nullptr;
    }
    if (epoll_fd_ != -1) {
        LogI("close epfd: %d", epoll_fd_)
        ::close(epoll_fd_);
        epoll_fd_ = -1;
    }
#endif
}


EpollNotifier::EpollNotifier()
        : socket_(INVALID_SOCKET)
        , socket_epoll_(nullptr) {
    
    if (socket_.Create(AF_INET, SOCK_STREAM) < 0) {
        LogE("create socket error: %s, errno: %d",
             strerror(errno), errno);
        return;
    }
    LogI("notify fd: %d", socket_.FD())
    socket_.SetNonblocking();
}

void EpollNotifier::SetSocketEpoll(SocketEpoll *_epoll) {
    assert(!socket_epoll_);
    if (_epoll) {
        socket_epoll_ = _epoll;
        socket_epoll_->AddSocketRead(socket_.FD());
    }
}

void EpollNotifier::NotifyEpoll(Notification &_notification) {
    assert(socket_epoll_);
    socket_epoll_->ModSocketWrite(socket_.FD(), (uint64_t) _notification.notify_id_);
}

SOCKET EpollNotifier::GetNotifyFd() const { return socket_.FD(); }

EpollNotifier::~EpollNotifier() {
    SOCKET fd = socket_.FD();
    if (fd != INVALID_SOCKET && socket_epoll_) {
        socket_epoll_->DelSocket(fd);
    }
}

EpollNotifier::Notification::Notification()
        : notify_id_(&notify_id_) {
}

EpollNotifier::Notification::Notification(EpollNotifier::Notification::NotifyId _notify_id)
        : notify_id_(_notify_id) {
}

bool EpollNotifier::Notification::operator==(const EpollNotifier::Notification &another) {
    return notify_id_ == another.notify_id_;
}
