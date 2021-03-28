#ifndef OI_SVR_SIGNALHANDLER_H
#define OI_SVR_SIGNALHANDLER_H
#include "singleton.h"
#include <vector>
#include <mutex>


class IProcessExitListener {
  public:
    IProcessExitListener(void (*_on_process_exit)());
    void (*OnProcessExit)();
};

class SignalHandler {
    
    SINGLETON(SignalHandler, )

  public:
    ~SignalHandler();
    
    void Init();
    
    int RegisterExitCallback(IProcessExitListener *_listener);
    
    void Handle(int _sig);

  private:
    void __ProcessExitManually();
    
    void __ProcessCrash();
    
    void __InvokeCallbacks();
    
    
  private:
    using ScopedLock = std::unique_lock<std::mutex>;
    std::mutex                              mutex_;
    std::vector<IProcessExitListener *>     process_exit_listeners_;
    
};


#endif //OI_SVR_SIGNALHANDLER_H
