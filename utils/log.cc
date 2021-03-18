#include "log.h"
#ifdef DAEMON
#ifdef __linux__
#include "signalhandler.h"
#endif
#endif

namespace Logger {
    
void OpenLog(const char *_ident) {
#ifdef DAEMON
#ifdef __linux__
    openlog(_ident, LOG_PID, LOG_USER);
    SignalHandler::Instance().RegisterExitCallback(
            new IProcessExitListener([] {
        closelog();
    }));
#endif
#endif
}
    
}

