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
    
    bool CheckHeartbeat(const tcp::RecvContext::Ptr&);
    
    const char *ConfigFile() override;
    
    ~ReverseProxyServer() override;
    
    WebServerProfile *LoadBalance(std::string &_ip);
    
    void ReportWebServerDown(WebServerProfile *);
    
    class ProxyConfig : public ServerConfigBase {
      public:
        ProxyConfig();
        ~ProxyConfig();
        static const char *const        key_load_balance_rule;
        static const char *const        key_webservers;
        static const char *const        key_ip;
        static const char *const        key_port;
        static const char *const        key_weight;
        static const char *const        kLoadBalanceRulePoll;
        static const char *const        kLoadBalanceRuleWeight;
        static const char *const        kLoadBalanceRuleIpHash;
        std::vector<WebServerProfile *> webservers;
        std::string                     load_balance_rule;
    };
    
    class NetThread : public NetThreadBase {
      public:
        NetThread();
        
        ~NetThread() override;
        
        void ConfigApplicationLayer(tcp::ConnectionProfile *) override;
    
        bool HandleApplicationPacket(tcp::RecvContext::Ptr) override;
        
        bool HandleHttpPacket(const tcp::RecvContext::Ptr&);
        
        bool HandleHttpRequest(const tcp::RecvContext::Ptr&);
        
        bool HandleHttpResponse(const tcp::RecvContext::Ptr&);
        
        bool HandleWebSocketPacket(const tcp::RecvContext::Ptr&);
    
        bool HandleForwardFailed(const tcp::RecvContext::Ptr&);
    
      protected:
      
      private:
        std::map<uint32_t, std::pair<uint32_t,
                        tcp::SendContext::Ptr>> conn_map_;
    };
    
  protected:
    bool _CustomConfig(yaml::YamlDescriptor *_desc) override;
    
    ServerConfigBase *_MakeConfig() override;

  private:
    static const char* const        kConfigFile;
    LoadBalancer                    load_balancer_;
};

