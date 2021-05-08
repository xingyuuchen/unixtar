#include "loadbalancer.h"
#include "log.h"
#include <cassert>


LoadBalancer::LoadBalancer()
        : balance_rule_(kPoll)
        , curr_weight_(0) {
    
    auto *profile = new WebServerProfile();
    profile->ip = "127.0.0.1";
    profile->port = 5002;
    profile->weight = 1;
    web_servers_.emplace_back(profile);
    last_selected_ = web_servers_.begin();
    curr_weight_ = (*last_selected_)->weight;
}

WebServerProfile *LoadBalancer::Select(std::string &_ip) {
    WebServerProfile *ret = nullptr;
    if (balance_rule_ == kPoll) {
        ret = __BalanceByPoll();
    } else if (balance_rule_ == kWeight) {
        ret = __BalanceByWeight();
    } else if (balance_rule_ == kIpHash) {
        ret = __BalanceByIpHash(_ip);
    } else {
        LogE("wtf is the rule? %d", balance_rule_)
    }
    return ret ? ret : web_servers_.front();
}

LoadBalancer::~LoadBalancer() {
    for (auto &profile : web_servers_) {
        delete profile, profile = nullptr;
    }
}

void LoadBalancer::RegisterWebServer(std::string &_ip,
                                     uint16_t _port,
                                     uint16_t _weight/* = 1*/) {
    auto *neo = new WebServerProfile();
    neo->ip = std::string(_ip);
    neo->port = _port;
    neo->weight = _weight;
    web_servers_.push_back(neo);
}


LoadBalancer::TBalanceRule LoadBalancer::BalanceRule() const {
    return balance_rule_;
}

void LoadBalancer::ConfigRule(LoadBalancer::TBalanceRule _rule) {
    if (_rule != kPoll && _rule != kWeight && _rule != kIpHash) {
        LogE("wtf is the rule? %d", _rule)
        assert(false);
    }
    balance_rule_ = _rule;
}

WebServerProfile *LoadBalancer::__BalanceByPoll() {
    ++last_selected_;
    if (last_selected_ == web_servers_.end()) {
        last_selected_ = web_servers_.begin();
    }
    return *last_selected_;
}

WebServerProfile *LoadBalancer::__BalanceByWeight() {
    if (curr_weight_-- == 0) {
        ++last_selected_;
        if (last_selected_ == web_servers_.end()) {
            last_selected_ = web_servers_.begin();
        }
        curr_weight_ = (*last_selected_)->weight;
    }
    return *last_selected_;
}

WebServerProfile *LoadBalancer::__BalanceByIpHash(std::string &_ip) {
    // TODO
    return nullptr;
}
