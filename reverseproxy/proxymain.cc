#include "proxyserver.h"
#include "log.h"


int main(int ac, char **argv) {

    LogI("Launching Proxy Server...")
    
    ReverseProxyServer::Instance().Config();
    
    ReverseProxyServer::Instance().Serve();
    
    LogI("Proxy Server Down")
    
    return 0;
}

