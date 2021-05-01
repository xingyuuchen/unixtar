#include "socketpoll.h"
#include <cerrno>


SocketPoll::SocketPoll()
    : errno_(0) {}


int SocketPoll::Poll() {
    return Poll(-1);
}


int SocketPoll::Poll(int _msec) {
    if (_msec < -1) { _msec = -1; }
    ClearEvents();
    
    int ret = poll(&pollfds_[0], (nfds_t) pollfds_.size(), _msec);
    if (ret < 0) {
        errno_ = errno;
    }
    return ret;
}

void SocketPoll::SetEventRead(SOCKET _socket) {
    if (_socket < 0) {
        return;
    }
    std::unique_lock<std::mutex> lock(mutex_);
    auto find = __FindPollfd(_socket);
    if (find != pollfds_.end()) {
        find->events |= POLLIN;
        return;
    }
    
    pollfd fd;
    fd.fd = _socket;
    fd.events = POLLIN;
    fd.revents = 0;
    pollfds_.push_back(fd);
}

void SocketPoll::SetEventWrite(SOCKET _socket) {
    if (_socket < 0) {
        return;
    }
    std::unique_lock<std::mutex> lock(mutex_);
    auto find = __FindPollfd(_socket);
    if (find != pollfds_.end()) {
        find->events |= POLLOUT;
        return;
    }
    
    pollfd fd;
    fd.fd = _socket;
    fd.events = POLLOUT;
    fd.revents = 0;
    pollfds_.push_back(fd);
}

void SocketPoll::SetEventError(SOCKET _socket) {
    if (_socket < 0) {
        return;
    }
    std::unique_lock<std::mutex> lock(mutex_);
    auto find = __FindPollfd(_socket);
    if (find != pollfds_.end()) {
        find->events |= POLLERR;
        return;
    }
    
    pollfd fd;
    fd.fd = _socket;
    fd.events = POLLERR;
    fd.revents = 0;
    pollfds_.push_back(fd);
}

void SocketPoll::ClearEvents() {
    std::unique_lock<std::mutex> lock(mutex_);
    for (pollfd &fd : pollfds_) {
        fd.revents = 0;
    }
}

bool SocketPoll::IsReadSet(SOCKET _socket) {
    std::unique_lock<std::mutex> lock(mutex_);
    auto find = __FindPollfd(_socket);
    if (find == pollfds_.end()) {
        return false;
    }
    return find->revents & POLLIN;
}

bool SocketPoll::IsErrSet(SOCKET _socket) {
    std::unique_lock<std::mutex> lock(mutex_);
    auto find = __FindPollfd(_socket);
    if (find == pollfds_.end()) {
        return false;
    }
    return find->revents & POLLERR;
}

void SocketPoll::RemoveSocket(SOCKET _socket) {
    std::unique_lock<std::mutex> lock(mutex_);
    auto find = __FindPollfd(_socket);
    if (find != pollfds_.end()) {
        pollfds_.erase(find);
    }
}

int SocketPoll::GetErrno() const {
    return errno_;
}

