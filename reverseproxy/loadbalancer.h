#pragma once
#include <string>
#include <vector>


struct WebServerProfile {
    WebServerProfile();
    std::string     ip;
    uint16_t        port;
    uint16_t        weight;
    uint64_t        last_down_ts;
    bool            is_down;
    static uint64_t MakeSeq();
    static uint64_t kInvalidSeq;
    uint64_t        svr_id;
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
    
    static void ReportWebServerDown(WebServerProfile *);
    
    TBalanceRule BalanceRule() const;

  private:
    static bool __IsWebServerAvailable(WebServerProfile *) ;
    
    WebServerProfile *__BalanceByPoll();
    
    WebServerProfile *__BalanceByWeight();
    
    WebServerProfile *__BalanceByIpHash(std::string &_ip);
    
  private:
    static const uint64_t                       kDefaultRetryPeriod;
    TBalanceRule                                balance_rule_;
    std::vector<WebServerProfile *>             web_servers_;
    std::vector<WebServerProfile *>::iterator   last_selected_;
    uint16_t                                    curr_weight_;
};

