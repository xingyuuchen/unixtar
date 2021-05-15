#include "loadbalancer.h"
#include "log.h"
#include "timeutil.h"
#include <cassert>
#include <cerrno>
#include <arpa/inet.h>


// 10 min
const uint64_t LoadBalancer::kDefaultRetryPeriod = 10 * 60 * 1000;

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

void LoadBalancer::ReportWebServerDown(WebServerProfile *_down_svr) {
    _down_svr->is_down = true;
    _down_svr->last_down_ts = ::gettickcount();
}

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
    } while (!__IsWebServerAvailable(*last_selected_));
    return *last_selected_;
}

WebServerProfile *LoadBalancer::__BalanceByWeight() {
    if (curr_weight_-- == 0) {
        do {
            ++last_selected_;
            if (last_selected_ == web_servers_.end()) {
                last_selected_ = web_servers_.begin();
            }
        } while (!__IsWebServerAvailable(*last_selected_));
        
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

bool LoadBalancer::__IsWebServerAvailable(WebServerProfile *_svr) {
    if (_svr->weight <= 0) {
        return false;
    }
    if (!_svr->is_down) {
        return true;
    }
    uint64_t now = ::gettickcount();
    if (_svr->last_down_ts + kDefaultRetryPeriod >= now) {
        // just trying.
        // If it is still down, a call to ReportWebServerDown() is necessary.
        _svr->is_down = false;
        return true;
    }
    return false;
}

LoadBalancer::~LoadBalancer() = default;


uint64_t WebServerProfile::kInvalidSeq = 0;

WebServerProfile::WebServerProfile()
    : svr_id(MakeSeq())
    , port(0)
    , weight(0)
    , is_down(false)
    , backlog(0)
    , last_down_ts(0)
    , last_heartbeat_ts(0) {
    
}

uint64_t WebServerProfile::MakeSeq() {
    static uint64_t seq = kInvalidSeq;
    return ++seq;
}
