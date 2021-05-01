#ifndef OI_SVR_LOG_H
#define OI_SVR_LOG_H

#include <syslog.h>
#include <execinfo.h>
#include <cstring>
#ifndef DAEMON
#include "timeutil.h"
#include <cstdio>
#endif

namespace logger {

/**
 * Advance configuration is necessary:
 *      1. Append your log configuration(log file location, priority, etc.) to /etc/rsyslog.conf;
 *      2. systemctl restart rsyslog.
 *
 * See /etc/rsyslog.conf for detail.
 *
 */
void OpenLog(const char *_ident);

void LogPrintStacktraceImpl(int _size = 8);

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
    
#define LogD(_fmt, ...) __LogD("D/%s [%s]: " _fmt, strrchr(__FILE__, '/') + 1, __func__, ##__VA_ARGS__)
#define LogI(_fmt, ...) __LogI("I/%s [%s]: " _fmt, strrchr(__FILE__, '/') + 1, __func__, ##__VA_ARGS__)
#define LogW(_fmt, ...) __LogW("W/%s [%s]: " _fmt, strrchr(__FILE__, '/') + 1, __func__, ##__VA_ARGS__)
#define LogE(_fmt, ...) __LogE("E/%s [%s]: " _fmt, strrchr(__FILE__, '/') + 1, __func__, ##__VA_ARGS__)

#endif
#else

#define LogD(...) printcurrtime(); \
    printf(" D/%s [%s]: ", strrchr(__FILE__, '/') + 1, __func__); \
    printf(__VA_ARGS__); \
    printf("\n");
#define LogI(...) printcurrtime(); \
    printf(" I/%s [%s]: ", strrchr(__FILE__, '/') + 1, __func__); \
    printf(__VA_ARGS__); \
    printf("\n");
#define LogW(...) printcurrtime(); \
    printf(" W/%s [%s]: ", strrchr(__FILE__, '/') + 1, __func__); \
    printf(__VA_ARGS__); \
    printf("\n");
#define LogE(...) printcurrtime(); \
    printf(" E/%s [%s]: ", strrchr(__FILE__, '/') + 1, __func__); \
    printf(__VA_ARGS__); \
    printf("\n");
#endif

#define LogPrintStacktrace(...)  logger::LogPrintStacktraceImpl(__VA_ARGS__);

#endif //OI_SVR_LOG_H
