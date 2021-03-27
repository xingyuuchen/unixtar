#pragma once
#include <mutex>
#include <queue>
#include <condition_variable>


namespace MessageQueue {


template<class T>
class ThreadSafeQueue {
  public:
    
    using ScopedLock = std::unique_lock<std::mutex>;
    
    using refrence = typename std::queue<T>::reference;  // T&
    
    explicit ThreadSafeQueue(size_t _max_size = 10240);
    
    bool push(const T &_v, bool _notify = true);
    
    bool front(T &_t, bool _wait = true, uint64_t _timeout_millis = 1000);
    
    void pop();
    
    bool pop_front(T &_t, bool _wait = true, uint64_t _timeout_millis = 1000);
    
    size_t size();

  private:
    size_t                      max_size_;
    std::mutex                  mtx_;
    std::condition_variable     cond_;
    std::queue<T>               queue_;
};

template<class T>
ThreadSafeQueue<T>::ThreadSafeQueue(size_t _max_size)
        : max_size_(_max_size) {
}

template<class T>
bool ThreadSafeQueue<T>::push(const T &_v, bool _notify /*= true*/) {
    ScopedLock lock(mtx_);
    if (queue_.size() > max_size_) {
        return false;
    }
    queue_.push(_v);
    if (_notify) {
        cond_.notify_one();
    }
    return true;
}


template<class T>
void ThreadSafeQueue<T>::pop() {
    ScopedLock lock(mtx_);
    if (!queue_.empty()) {
        queue_.pop();
    }
}


template<class T>
size_t ThreadSafeQueue<T>::size() {
    ScopedLock lock(mtx_);
    return queue_.size();
}


template<class T>
bool
ThreadSafeQueue<T>::front(T &_t, bool _wait /*= true*/,
                          uint64_t _timeout_millis /*= 1000*/) {
    ScopedLock lock(mtx_);
    if (_wait) {
        cond_.wait_for(lock,
                       std::chrono::milliseconds(_timeout_millis),
                       [this] { return !queue_.empty(); });
    }
    if (!queue_.empty()) {
        _t = queue_.front();
        return true;
    }
    return false;
}


template<class T>
bool
ThreadSafeQueue<T>::pop_front(T &_t, bool _wait /*= true*/,
                              uint64_t _timeout_millis /*= 1000*/) {
    ScopedLock lock(mtx_);
    if (_wait) {
        cond_.wait_for(lock,
                       std::chrono::milliseconds(_timeout_millis),
                       [this] { return !queue_.empty(); });
    }
    if (!queue_.empty()) {
        _t = queue_.front();
        queue_.pop();
        return true;
    }
    return false;
}

}
