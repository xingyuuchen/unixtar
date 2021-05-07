#pragma once
#include "httpserver.h"
#include "singleton.h"
#include "loadbalancer.h"
#include <string>
#include <map>



class ReverseProxyServer final : public HttpServer {

    SINGLETON(ReverseProxyServer, )

  public:
    const char *ConfigFile() override;
    
    ~ReverseProxyServer() override;
    
    LoadBalancer &GetLoadBalancer();
    
    class ProxyConfig : public ServerConfigBase {
      public:
        ProxyConfig();
        
    };
    
    class NetThread : public HttpNetThread {
      public:
        NetThread();
        
        ~NetThread() override;
        
        int HandleHttpRequest(tcp::ConnectionProfile *) override;
    
      protected:
      
      private:
        std::map<uint32_t, uint32_t> conn_map_;
    };
    
  protected:
    bool _CustomConfig(yaml::YamlDescriptor &_desc) override;
    
    void AfterConfig() override;
    
    ServerConfigBase *_MakeConfig() override;

  private:
    static const char* const        kConfigFile;
    LoadBalancer                    load_balancer_;
};

