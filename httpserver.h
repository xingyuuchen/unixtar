#pragma once
#include "serverbase.h"


class HttpServer : public ServerBase {
  public:
    HttpServer();
    
    ~HttpServer() override;
    
    class HttpNetThread : public NetThreadBase {
      public:
        HttpNetThread();
    
        ~HttpNetThread() override;
        
        virtual int HandleHttpPacket(tcp::ConnectionProfile *) = 0;

      protected:
        int _OnReadEvent(tcp::ConnectionProfile *) final;
    
        int _OnWriteEvent(tcp::SendContext *, bool _del) final;
    
    };
    
  protected:

  private:
};

