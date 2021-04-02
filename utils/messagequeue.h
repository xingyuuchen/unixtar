#pragma once
#include <mutex>
#include <deque>
#include <condition_variable>


namespace MessageQueue {


template<class T, class Container = std::deque<T>>
class ThreadSafeDeque {
  public:
    
    using UniqueLock = std::unique_lock<std::mutex>;
    using LockGuard = std::lock_guard<std::mutex>;
    using refrence = typename Container::reference;  // T&
    
    explicit ThreadSafeDeque(size_t _max_size = 10240);
    
    bool push_front(const T &_v, bool _notify = true);
    
    bool push_back(const T &_v, bool _notify = true);
    
    bool pop_front(bool _wait = true, uint64_t _timeout_millis = 1000);
    
    bool pop_back(bool _wait = true, uint64_t _timeout_millis = 1000);
    
    bool pop_front_to(T &_t, bool _wait = true, uint64_t _timeout_millis = 1000);
    
    bool pop_back_to(T &_t, bool _wait = true, uint64_t _timeout_millis = 1000);
    
    bool front(T &_t, bool _wait = true, uint64_t _timeout_millis = 1000);
    
    bool back(T &_t, bool _wait = true, uint64_t _timeout_millis = 1000);
    
    bool get(T &_t, size_t _pos, bool _wait = true, uint64_t _timeout_millis = 1000);
    
    size_t size();
    
    void clear();
    
    void Terminate();
    
    bool IsTerminated();

  private:
    void __WaitSizeGreaterThan(size_t _than, UniqueLock &_lock, uint64_t _timeout_millis);

  private:
    size_t                      max_size_;
    std::mutex                  mtx_;
    std::condition_variable     cond_;
    Container                   container_;
    bool                        terminated_;
};

template<class T, class Container>
ThreadSafeDeque<T, Container>::ThreadSafeDeque(size_t _max_size)
        : max_size_(_max_size)
        , terminated_(false) {
}


template<class T, class Container>
bool ThreadSafeDeque<T, Container>::push_front(const T &_v, bool _notify) {
    LockGuard lock(mtx_);
    if (container_.size() > max_size_) {
        return false;
    }
    container_.push_front(_v);
    if (_notify) {
        cond_.notify_one();
    }
    return true;
}


template<class T, class Container>
bool
ThreadSafeDeque<T, Container>::push_back(const T &_v, bool _notify /*= true*/) {
    LockGuard lock(mtx_);
    if (container_.size() > max_size_) {
        return false;
    }
    container_.push_back(_v);
    if (_notify) {
        cond_.notify_one();
    }
    return true;
}

template<class T, class Container>
bool ThreadSafeDeque<T, Container>::pop_front(bool _wait,
                                              uint64_t _timeout_millis) {
    UniqueLock lock(mtx_);
    if (_wait) {
        __WaitSizeGreaterThan(0, lock, _timeout_millis);
    }
    if (container_.empty() || terminated_) {
        return false;
    }
    container_.pop_front();
    return true;
}

template<class T, class Container>
bool ThreadSafeDeque<T, Container>::pop_back(bool _wait,
                                             uint64_t _timeout_millis) {
    UniqueLock lock(mtx_);
    if (_wait) {
        __WaitSizeGreaterThan(0, lock, _timeout_millis);
    }
    if (container_.empty() || terminated_) {
        return false;
    }
    container_.pop_back();
    return true;
}


template<class T, class Container>
bool
ThreadSafeDeque<T, Container>::pop_front_to(T &_t,
                                            bool _wait /*= true*/,
                                            uint64_t _timeout_millis /*= 1000*/) {
    UniqueLock lock(mtx_);
    if (_wait) {
        __WaitSizeGreaterThan(0, lock, _timeout_millis);
    }
    if (container_.empty() || terminated_) {
        return false;
    }
    _t = container_.front();
    container_.pop_front();
    return true;
}


template<class T, class Container>
bool
ThreadSafeDeque<T, Container>::pop_back_to(T &_t,
                                           bool _wait /*= true*/,
                                           uint64_t _timeout_millis /*= 1000*/) {
    UniqueLock lock(mtx_);
    if (_wait) {
        __WaitSizeGreaterThan(0, lock, _timeout_millis);
    }
    if (container_.empty() || terminated_) {
        return false;
    }
    _t = container_.back();
    container_.pop_back();
    return true;
}

template<class T, class Container>
bool
ThreadSafeDeque<T, Container>::front(T &_t, bool _wait /*= true*/,
                                     uint64_t _timeout_millis /*= 1000*/) {
    UniqueLock lock(mtx_);
    if (_wait) {
        __WaitSizeGreaterThan(0, lock, _timeout_millis);
    }
    if (container_.empty() || terminated_) {
        return false;
    }
    _t = container_.front();
    return true;
}

template<class T, class Container>
bool
ThreadSafeDeque<T, Container>::back(T &_t, bool _wait /*= true*/,
                                    uint64_t _timeout_millis /*= 1000*/) {
    UniqueLock lock(mtx_);
    if (_wait) {
        __WaitSizeGreaterThan(0, lock, _timeout_millis);
    }
    if (container_.empty() || terminated_) {
        return false;
    }
    _t = container_.back();
    return true;
}

template<class T, class Container>
bool
ThreadSafeDeque<T, Container>::get(T &_t,
                                   size_t _pos,
                                   bool _wait /* = true*/,
                                   uint64_t _timeout_millis /* = 1000*/) {
    UniqueLock lock(mtx_);
    if (_wait) {
        __WaitSizeGreaterThan(_pos, lock, _timeout_millis);
    }
    if (container_.size() <= _pos || terminated_) {
        return false;
    }
    _t = container_[_pos];
    return true;
}

template<class T, class Container>
size_t ThreadSafeDeque<T, Container>::size() {
    LockGuard lock(mtx_);
    return container_.size();
}

template<class T, class Container>
void ThreadSafeDeque<T, Container>::clear() {
    LockGuard lock(mtx_);
    container_.clear();
}

template<class T, class Container>
void ThreadSafeDeque<T, Container>::Terminate() {
    LockGuard lock(mtx_);
    terminated_ = true;
    cond_.notify_all();
}

template<class T, class Container>
bool ThreadSafeDeque<T, Container>::IsTerminated() {
    LockGuard lock(mtx_);
    return terminated_;
}

template<class T, class Container>
void
ThreadSafeDeque<T, Container>::__WaitSizeGreaterThan(const size_t _than,
                                                     ThreadSafeDeque::UniqueLock &_lock,
                                                     uint64_t _timeout_millis) {
    cond_.wait_for(_lock,
                   std::chrono::milliseconds(_timeout_millis),
                   [&, this] { return container_.size() > _than || terminated_; });
}

}
