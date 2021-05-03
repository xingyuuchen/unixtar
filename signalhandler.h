#ifndef OI_SVR_SIGNALHANDLER_H
#define OI_SVR_SIGNALHANDLER_H
#include "singleton.h"
#include <signal.h>
#include <stack>
#include <map>
#include <mutex>
#include <functional>


class SignalHandler {
    
    SINGLETON(SignalHandler, )

  public:
    ~SignalHandler();
    
    void Init();
    
    int RegisterCallback(int _sig, std::function<void()> _callback);
    
    void Handle(int _sig);

  private:
    void __ProcessCrash();
    
  private:
    using ScopedLock = std::unique_lock<std::mutex>;
    std::mutex                                          mutex_;
    std::map<int, std::stack<std::function<void()>>>    callback_map_;
    
};


#endif //OI_SVR_SIGNALHANDLER_H
