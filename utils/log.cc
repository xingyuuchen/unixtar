#include "log.h"
#include <stdlib.h>
#ifdef DAEMON
#ifdef __linux__
#include "signalhandler.h"
#endif
#endif

namespace logger {

void OpenLog(const char *_ident) {
#ifdef DAEMON
#ifdef __linux__
    openlog(_ident, LOG_PID, LOG_USER);
    SignalHandler::Instance().RegisterCallback(SIGINT, [] {
        LogI("log is closed!")
        closelog();
    });
#endif
#endif
}

void LogPrintStacktraceImpl(int _size /* = 8*/) {
    if (_size < 4) {
        _size = 4;
    }
    if (_size > 32) {
        _size = 32;
    }
    void *array[_size];
    int stack_num = backtrace(array, _size);
    char **stacktrace = backtrace_symbols(array, stack_num);
    for (int i = 0; i < stack_num; ++i) {
#ifdef DAEMON
        syslog(LOG_INFO, stacktrace[i]);
#else
        printf("%d: %s\n", i, stacktrace[i]);
#endif
    }
    free(stacktrace);
}

}
