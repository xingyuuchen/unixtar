#include "signalhandler.h"
#include "utils/log.h"
#include <stdlib.h>
#include <signal.h>


IProcessExitListener::IProcessExitListener(void (*_on_process_exit)())
    : OnProcessExit(_on_process_exit) { }

    
void SignalHandler::Init() {}

void SignalHandlerBridge(int _sig) {
    SignalHandler::Instance().Handle(_sig);
}

SignalHandler::SignalHandler() {
    LogI(__FILE__, "[SignalHandler]")
    
    ::signal(SIGINT, SignalHandlerBridge);
    ::signal(SIGQUIT, SignalHandlerBridge);
    ::signal(SIGSEGV, SignalHandlerBridge);
    ::signal(SIGBUS, SignalHandlerBridge);
    ::signal(SIGFPE, SignalHandlerBridge);
    ::signal(SIGSYS, SignalHandlerBridge);
}

int SignalHandler::RegisterExitCallback(IProcessExitListener *_listener) {
    process_exit_listeners_.push_back(_listener);
    LogI(__FILE__, "[RegisterExitCallback] n_callbacks: %zd",
         process_exit_listeners_.size())
    return 0;
}

void SignalHandler::Handle(int _sig) {
    LogI(__FILE__, "[ProcessExit] sig(%d)", _sig)
    switch (_sig) {
        case SIGINT:
        case SIGQUIT:
            __ProcessExitManually();
            break;
        case SIGSEGV:
        case SIGBUS:
        case SIGFPE:
            __ProcessCrash();
            break;
        default:
            __ProcessCrash();
    }
}

void SignalHandler::__InvokeCallbacks() {
    LogI(__FILE__, "[__InvokeCallbacks]")
    static bool invoked = false;
    if (!invoked) {
        LogI(__FILE__, "[__InvokeCallbacks] n_callbacks: %zd", process_exit_listeners_.size())
        for (IProcessExitListener *listener : process_exit_listeners_) {
            if (listener->OnProcessExit) {
                LogI(__FILE__, "[__InvokeCallbacks] execute %p",
                     listener->OnProcessExit)
                listener->OnProcessExit();
            }
        }
        invoked = true;
    }
}

void SignalHandler::__ProcessExitManually() {
    LogI(__FILE__, "[__ProcessExitManually] Process Exit Manually.")
    exit(EXIT_SUCCESS);
}

void SignalHandler::__ProcessCrash() {
    // TODO: print error message
    LogI(__FILE__, "[__ProcessCrash] Process Crash!")
    exit(EXIT_FAILURE);
}

SignalHandler::~SignalHandler() {
    LogI(__FILE__, "[~SignalHandler]")
    __InvokeCallbacks();
    for (IProcessExitListener *p : process_exit_listeners_) {
        delete p, p = NULL;
    }
}
