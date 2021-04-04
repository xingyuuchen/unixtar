#include "socketepoll.h"
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "log.h"


const int SocketEpoll::kMaxFds = 1024;

SocketEpoll::SocketEpoll(int _max_fds)
        : epoll_fd_(-1)
        , listen_fd_(-1)
        , epoll_events_(NULL)
        , errno_(0)
{
#ifdef __linux__
    if (_max_fds <= 0) { return; }
    
    epoll_events_ = new struct epoll_event[_max_fds];
    
    int ret = ::epoll_create1(0);
    if (ret < 0) {
        LogE(__FILE__, "[SocketEpoll] epoll_create, ret = %d", ret)
        return;
    }
    epoll_fd_ = ret;
#endif
}


int SocketEpoll::AddSocketRead(int _fd) {
#ifdef __linux__
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLET;
    event.data.fd = _fd;
    return __EpollCtl(EPOLL_CTL_ADD, _fd, &event);
#else
    return 0;
#endif
}

int SocketEpoll::ModSocketWrite(int _fd, void *_ptr) {
#ifdef __linux__
    struct epoll_event event;
    event.events |= EPOLLOUT;
    event.data.ptr = _ptr;
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

int SocketEpoll::__EpollCtl(int _op, int _fd, struct epoll_event *_event/* = NULL*/) {
#ifdef __linux__
    if (_fd < 0) {
        LogE(__FILE__, "[__EpollCtl] fd_ < 0")
        return -1;
    }
    int ret = ::epoll_ctl(epoll_fd_, _op, _fd, _event);
    if (ret < 0) {
        errno_ = errno;
        LogE(__FILE__, "[__EpollCtl] errno(%d): %s", errno_, strerror(errno))
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
    
    int retry = 3;
    while (--retry) {
        int nfds = ::epoll_wait(epoll_fd_, epoll_events_, _max_events, _timeout_mills);
        if (nfds < 0) {
            if (errno == EINTR) {
                LogE(__FILE__, "[EpollWait] just EINTR, continue...")
                continue;
            }
            errno_ = errno;
            LogE(__FILE__, "[EpollWait] errno(%d): %s", errno_, strerror(errno))
        }
        return nfds;
    }
    return -1;
#else
    return 0;
#endif
}

bool SocketEpoll::IsNewConnect(int _idx) {
#ifdef __linux__
    if (_idx < 0 || _idx >= kMaxFds) {
        LogE(__FILE__, "[IsNewConnect] invalid _idx: %d", _idx)
        return false;
    }
    return epoll_events_[_idx].data.fd == listen_fd_;
#else
    return false;
#endif
}

void *SocketEpoll::IsWriteSet(int _idx) {
#ifdef __linux__
    epoll_data *ret = __IsFlagSet(_idx, EPOLLOUT);
    return ret == nullptr ? nullptr : ret->ptr;
#else
    return nullptr;
#endif
}

SOCKET SocketEpoll::GetSocket(int _idx) {
#ifdef __linux__
    if (_idx < 0 || _idx >= kMaxFds) {
        LogE(__FILE__, "[GetSocket] invalid _idx: %d", _idx)
        return INVALID_SOCKET;
    }
    return epoll_events_[_idx].data.fd;
#endif
    return INVALID_SOCKET;
}

void *SocketEpoll::GetEpollDataPtr(int _idx) {
#ifdef __linux__
    if (_idx < 0 || _idx >= kMaxFds) {
        LogE(__FILE__, "[GetSocket] invalid _idx: %d", _idx)
        return nullptr;
    }
    return epoll_events_[_idx].data.ptr;
#endif
    return nullptr;
}

int SocketEpoll::IsReadSet(int _idx) {
#ifdef __linux__
    epoll_data *ret = __IsFlagSet(_idx, EPOLLIN);
    return ret == NULL ? 0 : ret->fd;
#else
    return 0;
#endif
}

int SocketEpoll::IsErrSet(int _idx) {
#ifdef __linux__
    epoll_data *ret = __IsFlagSet(_idx, EPOLLERR);
    return ret == NULL ? 0 : ret->fd;
#else
    return 0;
#endif
}

epoll_data *SocketEpoll::__IsFlagSet(int _idx, int _flag) {
#ifdef __linux__
    if (_idx < 0 || _idx >= kMaxFds) {
        LogE(__FILE__, "[__IsFlagSet] invalid _idx: %d", _idx)
        return NULL;
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
        LogE(__FILE__, "[SetListenFd] _listen_fd: %d", _listen_fd)
        return;
    }
    AddSocketRead(_listen_fd);
    listen_fd_ = _listen_fd;
#endif
}

int SocketEpoll::GetErrNo() const { return errno_; }

SocketEpoll::~SocketEpoll() {
#ifdef __linux__
    if (epoll_events_ != NULL) {
        delete[] epoll_events_, epoll_events_ = NULL;
    }
    if (epoll_fd_ != -1) {
        LogI(__FILE__, "[~SocketEpoll] close epfd")
        ::close(epoll_fd_);
        epoll_fd_ = -1;
    }
#endif
}


EpollNotifier::EpollNotifier()
        : fd_(INVALID_SOCKET)
        , socket_epoll_(nullptr) {
    
    fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0) {
        LogE(__FILE__, "[EpollNotifier] create socket error: %s, errno: %d",
             strerror(errno), errno);
        return;
    }
    LogI(__FILE__, "[EpollNotifier] notify fd: %d", fd_)
    SetNonblocking(fd_);
}

void EpollNotifier::SetSocketEpoll(SocketEpoll *_epoll) {
    if (socket_epoll_) {
        LogE(__FILE__, "[SetSocketEpoll] socket_epoll_ already set!")
        return;
    }
    if (_epoll) {
        socket_epoll_ = _epoll;
        socket_epoll_->AddSocketRead(fd_);
    }
}

void EpollNotifier::NotifyEpoll(Notification &_notification) {
    if (socket_epoll_) {
        socket_epoll_->ModSocketWrite(fd_, const_cast<void *>(_notification.notify_id_));
    }
}

SOCKET EpollNotifier::GetNotifyFd() const { return fd_; }

EpollNotifier::~EpollNotifier() {
    if (fd_ != INVALID_SOCKET) {
        if (socket_epoll_) {
            socket_epoll_->DelSocket(fd_);
        }
        ::close(fd_), fd_ = INVALID_SOCKET;
    }
}

EpollNotifier::Notification::Notification()
        : notify_id_(&notify_id_) {
}

EpollNotifier::Notification::Notification(void *_notify_id)
        : notify_id_(_notify_id) {
}

bool EpollNotifier::Notification::operator==(const EpollNotifier::Notification &another) {
    return notify_id_ == another.notify_id_;
}
