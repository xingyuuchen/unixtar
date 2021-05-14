#pragma once
#include "serverbase.h"
#include "singleton.h"


class WebSocketServer : public ServerBase {
    
    SINGLETON(WebSocketServer, )
    
  public:
    
    ~WebSocketServer() override;
    
    void BeforeConfig() override;
    
    const char *ConfigFile() override;
    
    void AfterConfig() override;

    
    class NetThread : public NetThreadBase {
      public:
        NetThread();
        
        ~NetThread() override;
    
        int HandleApplicationPacket(tcp::ConnectionProfile *profile) override;
    
        void OnStart() override;
    
        void ConfigApplicationLayer(tcp::ConnectionProfile *) override;
    
        void OnStop() override;
    
        int HandShake(tcp::ConnectionProfile *);

      protected:
    };
    
  protected:
    
    ServerConfigBase *_MakeConfig() override;
    
    bool _CustomConfig(yaml::YamlDescriptor *_desc) override;
    
  private:
    static const char* const kConfigFile;
};

