#pragma once
#include <string>
#include <list>


struct WebServerProfile {
    std::string     ip;
    uint16_t        port;
    uint16_t        weight;
};

class LoadBalancer {
  public:
    enum TBalanceRule {
        kPoll = 0,
        kWeight,
        kIpHash,
    };
    
    LoadBalancer();
    
    ~LoadBalancer();
    
    void ConfigRule(TBalanceRule);
    
    WebServerProfile *Select();
    
    TBalanceRule BalanceRule() const;
    
  private:
    TBalanceRule                        balance_rule_;
    std::list<WebServerProfile *>       web_servers_;
};

