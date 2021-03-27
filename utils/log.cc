#include "log.h"
#include <stdlib.h>
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

void LogPrintStackTraceImpl() {
    int size = 16;
    void *array[16];
    int stack_num = backtrace(array, size);
    char **stacktrace = backtrace_symbols(array, stack_num);
    for (int i = 0; i < stack_num; ++i) {
#ifdef DAEMON
        syslog(LOG_INFO, stacktrace[i]);
#else
        printf("%s\n", stacktrace[i]);
#endif
    }
    free(stacktrace);
}

}
