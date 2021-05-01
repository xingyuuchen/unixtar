#include "signalhandler.h"
#include "utils/log.h"
#include <cstdlib>

    
void SignalHandler::Init() {}

static void SignalHandlerBridge(int _sig) {
    SignalHandler::Instance().Handle(_sig);
}

SignalHandler::SignalHandler() = default;


int SignalHandler::RegisterCallback(int _sig, std::function<void()> _callback) {
    if (_sig < 1 || _sig > 31) {
        return -1;
    }
    ScopedLock lock(mutex_);
    auto find = callback_map_.find(_sig);
    if (find == callback_map_.end()) {
        ::signal(_sig, SignalHandlerBridge);
    }
    callback_map_[_sig].push(_callback);
    return 0;
}

void SignalHandler::Handle(int _sig) {
    ScopedLock lock(mutex_);
    auto find = callback_map_.find(_sig);
    if (find != callback_map_.end()) {
        std::stack<std::function<void()>> &callbacks = find->second;
        while (!callbacks.empty()) {
            callbacks.top()();
            callbacks.pop();
        }
    }
}

// deprecated
void SignalHandler::__ProcessCrash() {
    // FIXME: LogI is not-reentrant
    LogI("Process Crash!")
    exit(EXIT_FAILURE);
}

SignalHandler::~SignalHandler() {
    LogI("[~SignalHandler]")
    
}
