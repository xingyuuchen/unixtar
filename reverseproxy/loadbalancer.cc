#include "loadbalancer.h"
#include "log.h"
#include <cassert>


LoadBalancer::LoadBalancer()
        : balance_rule_(kPoll) {
    
    auto *profile = new WebServerProfile();
    profile->ip = "127.0.0.1";
    profile->port = 5002;
    profile->weight = 1;
    web_servers_.emplace_back(profile);
}

WebServerProfile *LoadBalancer::Select() {
    if (balance_rule_ == kPoll) {
    
    } else if (balance_rule_ == kWeight) {
    
    } else if (balance_rule_ == kIpHash) {
    
    } else {
        LogE("wtf is the rule? %d", balance_rule_)
    }
    return web_servers_.front();
}

LoadBalancer::~LoadBalancer() {
    for (auto &profile : web_servers_) {
        delete profile, profile = nullptr;
    }
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
