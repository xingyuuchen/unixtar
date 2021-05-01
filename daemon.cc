#include "daemon.h"
#include <unistd.h>
#include <cstdio>
#include <fcntl.h>
#include <csignal>


namespace Daemon {

int Daemonize() {
    pid_t pid = fork();
    if (pid < 0) {
        printf("fork failed.\n");
        return -1;
    } else if (pid) {
        _exit(0);
    }
    
    if (setsid() < 0) {
        printf("setsid failed.\n");
        return -1;
    }
    
    signal(SIGHUP, SIG_IGN);
    
    if ((pid = fork()) < 0) {
        printf("fork failed.\n");
        return -1;
    } else if (pid) {
        _exit(0);
    }
    
//    chdir("/");
    
    for (int i = 0; i < 64; ++i) {
        close(i);
    }
    
    open("/dev/null", O_RDONLY);
    open("/dev/null", O_RDWR);
    open("/dev/null", O_RDWR);
    
    return 0;
}


}
