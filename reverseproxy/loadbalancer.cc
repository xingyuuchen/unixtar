#include "loadbalancer.h"
#include "log.h"
#include <cassert>
#include <cerrno>
#include <arpa/inet.h>


LoadBalancer::LoadBalancer()
        : balance_rule_(kUnknown)
        , curr_weight_(0) {
    
}

WebServerProfile *LoadBalancer::Select(std::string &_ip) {
    if (web_servers_.empty()) {
        return nullptr;
    }
    WebServerProfile *ret = nullptr;
    if (balance_rule_ == kPoll) {
        ret = __BalanceByPoll();
    } else if (balance_rule_ == kWeight) {
        ret = __BalanceByWeight();
    } else if (balance_rule_ == kIpHash) {
        ret = __BalanceByIpHash(_ip);
    } else {
        LogE("please configure Balance Rule first")
    }
    return ret ? ret : web_servers_.front();
}

LoadBalancer::~LoadBalancer() = default;

void LoadBalancer::RegisterWebServer(WebServerProfile *_webserver) {
    web_servers_.push_back(_webserver);
    last_selected_ = web_servers_.begin();
}


LoadBalancer::TBalanceRule LoadBalancer::BalanceRule() const {
    return balance_rule_;
}

void LoadBalancer::ConfigRule(LoadBalancer::TBalanceRule _rule) {
    assert(_rule == kPoll || _rule == kWeight || _rule == kIpHash);
    balance_rule_ = _rule;
}

WebServerProfile *LoadBalancer::__BalanceByPoll() {
    do {
        ++last_selected_;
        if (last_selected_ == web_servers_.end()) {
            last_selected_ = web_servers_.begin();
        }
    } while ((*last_selected_)->weight <= 0);
    return *last_selected_;
}

WebServerProfile *LoadBalancer::__BalanceByWeight() {
    if (curr_weight_-- == 0) {
        do {
            ++last_selected_;
            if (last_selected_ == web_servers_.end()) {
                last_selected_ = web_servers_.begin();
            }
        } while ((*last_selected_)->weight <= 0);
        curr_weight_ = (*last_selected_)->weight;
    }
    return *last_selected_;
}


// FIXME: get rid of webservers with weight 0.
WebServerProfile *LoadBalancer::__BalanceByIpHash(std::string &_ip) {
    uint32_t ip;
    int ret = inet_pton(AF_INET, _ip.c_str(), &ip);
    if (ret == 1) {
        size_t idx = ip % web_servers_.size();
        last_selected_ = web_servers_.begin() + (int) idx;
        curr_weight_ = (*last_selected_)->weight;
        return *last_selected_;
    } else if (ret == 0) {
        LogE("the address was not parseable in the specified address family")
    } else {
        LogE("ret: %d, errno(%d): %s", ret, errno, strerror(errno))
    }
    return nullptr;
}
