#include "proxyserver.h"
#include "log.h"
#ifdef DAEMON
#include "daemon.h"
#endif


int main(int ac, char **argv) {
#ifdef DAEMON
    if (unixtar::Daemonize() < 0) {
        printf("daemonize failed\n");
        return 0;
    }
#endif

    LogI("Launching Proxy Server...")
    
    ReverseProxyServer::Instance().Config();
    
    ReverseProxyServer::Instance().Serve();
    
    LogI("Proxy Server Down")
    
    return 0;
}

