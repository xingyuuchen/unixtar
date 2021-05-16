#pragma once
#include "serverbase.h"
#include "singleton.h"
#include "loadbalancer.h"
#include <string>
#include <map>



class ReverseProxyServer final : public ServerBase {

    SINGLETON(ReverseProxyServer, )

  public:
    void AfterConfig() override;
    
    bool CheckHeartbeat(tcp::ConnectionProfile *);
    
    const char *ConfigFile() override;
    
    ~ReverseProxyServer() override;
    
    WebServerProfile *LoadBalance(std::string &_ip);
    
    void ReportWebServerDown(WebServerProfile *);
    
    class ProxyConfig : public ServerConfigBase {
      public:
        ProxyConfig();
        ~ProxyConfig();
        static const char *const        key_webservers;
        static const char *const        key_ip;
        static const char *const        key_port;
        static const char *const        key_weight;
        std::vector<WebServerProfile *> webservers;
    };
    
    class NetThread : public NetThreadBase {
      public:
        NetThread();
        
        ~NetThread() override;
        
        void ConfigApplicationLayer(tcp::ConnectionProfile *) override;
    
        int HandleApplicationPacket(tcp::ConnectionProfile *) override;
    
        void HandleForwardFailed(tcp::ConnectionProfile *_client_conn);
    
      protected:
      
      private:
        std::map<uint32_t, uint32_t> conn_map_;
    };
    
  protected:
    bool _CustomConfig(yaml::YamlDescriptor *_desc) override;
    
    ServerConfigBase *_MakeConfig() override;

  private:
    static const char* const        kConfigFile;
    LoadBalancer                    load_balancer_;
};

