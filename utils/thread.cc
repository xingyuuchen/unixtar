#include "thread.h"
#include "log.h"


Thread::Thread()
        : thread_(nullptr)
        , running_(false) {
}

void Thread::Entry() {
    running_ = true;
    try {
        Run();
    } catch (std::exception &ex) {
        running_ = false;
        HandleException(ex);
    }
    running_ = false;
}

void Thread::Start() {
    running_ = true;
    thread_ = new std::thread(&Thread::Entry, this);
}

void Thread::Join() {
    if (thread_) {
        if (std::this_thread::get_id() == thread_->get_id()) {
            LogE(__FILE__, "[Join] cannot call join in the same thread")
            return;
        }
        if (thread_->joinable()) {
            thread_->join();
        }
    }
}

std::thread::id Thread::Tid() const {
    if (thread_) {
        return thread_->get_id();
    }
    return {};
}

void Thread::Detach() {
    if (thread_) {
        thread_->detach();
    }
}

Thread::~Thread() {
    if (thread_) {
        if (thread_->joinable()) {
            thread_->detach();
        }
        running_ = false;
        delete thread_, thread_ = nullptr;
    }
}

void Thread::HandleException(std::exception &ex) {
    // implement if needed
}

bool Thread::IsRunning() const { return running_; }
