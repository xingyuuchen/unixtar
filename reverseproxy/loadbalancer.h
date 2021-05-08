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
    
    void RegisterWebServer(std::string &_ip, uint16_t _port,
                           uint16_t _weight = 1);
    
    void ConfigRule(TBalanceRule);
    
    WebServerProfile *Select(std::string &_ip);
    
    TBalanceRule BalanceRule() const;

  private:
    WebServerProfile *__BalanceByPoll();
    
    WebServerProfile *__BalanceByWeight();
    
    WebServerProfile *__BalanceByIpHash(std::string &_ip);
    
  private:
    TBalanceRule                            balance_rule_;
    std::list<WebServerProfile *>           web_servers_;
    std::list<WebServerProfile *>::iterator last_selected_;
    uint16_t                                curr_weight_;
};

