#ifndef OI_SVR_HTTPSERVER_H
#define OI_SVR_HTTPSERVER_H
#include <stdint.h>
#include "netscenebase.h"
#include "singleton.h"


class HttpServer {
    
    SINGLETON(HttpServer, )
    
  public:
    
    ~HttpServer();
    
    void Run(uint16_t _port = 5002);
    

  private:
    
    int __CreateListenFd();
    
    int __Bind(uint16_t _port);
    
    int __HandleConnect();
    
    int __HandleRead(SOCKET _fd);
    int __HandleReadTest(SOCKET _fd);
    
    int __HandleWrite(NetSceneBase *_net_scene, bool _mod_write);
    
    int __HandleErr(SOCKET _fd);
    
  private:
    static const int            kBuffSize;
    bool                        running_;
    int                         listenfd_;
    
};


#endif //OI_SVR_HTTPSERVER_H
