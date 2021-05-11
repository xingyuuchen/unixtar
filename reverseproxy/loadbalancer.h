#pragma once
#include <string>
#include <vector>


struct WebServerProfile {
    std::string     ip;
    uint16_t        port;
    uint16_t        weight;
};

class LoadBalancer {
  public:
    enum TBalanceRule {
        kUnknown = 0,
        kPoll,
        kWeight,
        kIpHash,
    };
    
    LoadBalancer();
    
    ~LoadBalancer();
    
    void RegisterWebServer(WebServerProfile *);
    
    void ConfigRule(TBalanceRule);
    
    WebServerProfile *Select(std::string &_ip);
    
    TBalanceRule BalanceRule() const;

  private:
    WebServerProfile *__BalanceByPoll();
    
    WebServerProfile *__BalanceByWeight();
    
    WebServerProfile *__BalanceByIpHash(std::string &_ip);
    
  private:
    TBalanceRule                                balance_rule_;
    std::vector<WebServerProfile *>             web_servers_;
    std::vector<WebServerProfile *>::iterator   last_selected_;
    uint16_t                                    curr_weight_;
};

