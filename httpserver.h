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
    
        void ConfigApplicationLayer(tcp::ConnectionProfile *) override;

      protected:
        int _OnReadEvent(tcp::ConnectionProfile *) final;
    
        bool _OnWriteEvent(tcp::SendContext *) final;
    
    };
    
  protected:

  private:
};

