#ifndef OI_SVR_LOG_H
#define OI_SVR_LOG_H

#include <syslog.h>
#include <execinfo.h>
#ifndef DAEMON
#include "timeutil.h"
#include <stdio.h>
#endif

namespace Logger {

/**
 * Advanced configuration is necessary:
 *      1. Append your log configuration(log file location, priority, etc.) to /etc/rsyslog.conf;
 *      2. systemctl restart rsyslog.
 *
 * See /etc/rsyslog.conf for detail.
 *
 */
void OpenLog(const char *_ident);

void LogPrintStackTraceImpl();

}

#ifdef DAEMON
#ifdef __linux__
#define __LogD(_fmt, ...) do { \
        syslog(LOG_DEBUG, _fmt, ##__VA_ARGS__); \
    } while (false);
#define __LogI(_fmt, ...) do { \
        syslog(LOG_INFO, _fmt, ##__VA_ARGS__); \
    } while (false);
#define __LogW(_fmt, ...) do { \
        syslog(LOG_WARNING, _fmt, ##__VA_ARGS__); \
    } while (false);
#define __LogE(_fmt, ...) do { \
        syslog(LOG_ERR, _fmt, ##__VA_ARGS__); \
    } while (false);
    
#define LogD(TAG, _fmt, ...) __LogD("%s: " _fmt, TAG, ##__VA_ARGS__)
#define LogI(TAG, _fmt, ...) __LogI("%s: " _fmt, TAG, ##__VA_ARGS__)
#define LogW(TAG, _fmt, ...) __LogW("%s: " _fmt, TAG, ##__VA_ARGS__)
#define LogE(TAG, _fmt, ...) __LogE("%s: " _fmt, TAG, ##__VA_ARGS__)

#endif
#else

#define LogD(TAG, ...) printcurrtime(); \
    printf(" D/%s: ", TAG); \
    printf(__VA_ARGS__); \
    printf("\n");
#define LogI(TAG, ...) printcurrtime(); \
    printf(" I/%s: ", TAG); \
    printf(__VA_ARGS__); \
    printf("\n");
#define LogW(TAG, ...) printcurrtime(); \
    printf(" W/%s: ", TAG); \
    printf(__VA_ARGS__); \
    printf("\n");
#define LogE(TAG, ...) printcurrtime(); \
    printf(" E/%s: ", TAG); \
    printf(__VA_ARGS__); \
    printf("\n");
#endif

#define LogPrintStackTrace()  Logger::LogPrintStackTraceImpl();

#endif //OI_SVR_LOG_H
