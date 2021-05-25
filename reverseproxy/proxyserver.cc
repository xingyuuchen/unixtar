#include "proxyserver.h"
#include "log.h"
#include "http/httprequest.h"
#include "http/httpresponse.h"
#include "netscenesvrheartbeat.pb.h"


const char *const ReverseProxyServer::kConfigFile = "proxyserverconf.yml";
const char *const ReverseProxyServer::ProxyConfig::key_load_balance_rule = "load_balance_rule";
const char *const ReverseProxyServer::ProxyConfig::key_ip = "ip";
const char *const ReverseProxyServer::ProxyConfig::key_port = "port";
const char *const ReverseProxyServer::ProxyConfig::key_weight = "weight";
const char *const ReverseProxyServer::ProxyConfig::key_webservers = "webservers";
const char *const ReverseProxyServer::ProxyConfig::kLoadBalanceRulePoll = "poll";
const char *const ReverseProxyServer::ProxyConfig::kLoadBalanceRuleWeight = "weight";
const char *const ReverseProxyServer::ProxyConfig::kLoadBalanceRuleIpHash = "ip_hash";


ReverseProxyServer::ReverseProxyServer()
        : ServerBase() {
    
    load_balancer_.ConfigRule(LoadBalancer::kPoll);
}


WebServerProfile *ReverseProxyServer::LoadBalance(std::string &_ip) {
    return load_balancer_.Select(_ip);
}

void ReverseProxyServer::ReportWebServerDown(WebServerProfile *_down_svr) {
    load_balancer_.ReportWebServerDown(_down_svr);
}

ServerBase::ServerConfigBase *ReverseProxyServer::_MakeConfig() {
    return new ProxyConfig();
}

ReverseProxyServer::ProxyConfig::ProxyConfig()
        : ServerConfigBase() {
}

ReverseProxyServer::ProxyConfig::~ProxyConfig() {
    for (auto & webserver : webservers) {
        delete webserver, webserver = nullptr;
    }
}


ReverseProxyServer::NetThread::NetThread()
        : NetThreadBase() {
}

void ReverseProxyServer::AfterConfig() {
    SetNetThreadImpl<NetThread>();
    
    auto *conf = (ProxyConfig *) config_;
    for (auto & webserver : conf->webservers) {
        load_balancer_.RegisterWebServer(webserver);
    }
    LogI("register %ld webservers to load balancer", conf->webservers.size())
}

void ReverseProxyServer::NetThread::ConfigApplicationLayer(
                    tcp::ConnectionProfile *_conn) {
    tcp::TConnectionType type = _conn->GetType();
    if (type == tcp::kAcceptFrom) {
        _conn->ConfigApplicationLayer<http::request::HttpRequest,
                                      http::request::Parser>();
    } else if (type == tcp::kConnectTo) {
        _conn->ConfigApplicationLayer<http::response::HttpResponse,
                                      http::response::Parser>();
    } else {
        LogE("unknown type: %d", type)
    }
}

bool ReverseProxyServer::NetThread::HandleApplicationPacket(
                    tcp::RecvContext::Ptr _recv_ctx) {
    assert(_recv_ctx);
    if (_recv_ctx->application_packet->Protocol() == kHttp1_1) {
        return HandleHttpPacket(_recv_ctx);
        
    } else if (_recv_ctx->application_packet->Protocol() == kWebSocket) {
        return HandleWebSocketPacket(_recv_ctx);
    }
    LogI("not http nor ws, delete connection")
    DelConnection(_recv_ctx->tcp_connection_uid);
    return true;
}

bool ReverseProxyServer::NetThread::HandleHttpPacket(
                    const tcp::RecvContext::Ptr& _recv_ctx) {
    tcp::TConnectionType type = _recv_ctx->type;
    if (type == tcp::kAcceptFrom) {
        // Forward request to webserver or Handle heartbeat from webserver.
        return HandleHttpRequest(_recv_ctx);
        
    } else if (type == tcp::kConnectTo) {
        // Send back response to client.
        return HandleHttpResponse(_recv_ctx);
    }
    LogE("unknown type: %d", type)
    DelConnection(_recv_ctx->tcp_connection_uid);
    return true;
}

bool ReverseProxyServer::NetThread::HandleHttpRequest(
                    const tcp::RecvContext::Ptr &_recv_ctx) {
    assert(_recv_ctx->type == tcp::kAcceptFrom);
    uint32_t uid = _recv_ctx->tcp_connection_uid;
    
    if (ReverseProxyServer::Instance().CheckHeartbeat(_recv_ctx)) {
        DelConnection(uid);
        return true;
    }
    
    std::string src_ip = _recv_ctx->from_ip;
    tcp::ConnectionProfile *conn_to_webserver;
    
    while (true) {
        auto forward_host = ReverseProxyServer::Instance().LoadBalance(src_ip);
        if (!forward_host) {
            LogE("no web server to forward")
            return HandleForwardFailed(_recv_ctx);
        }
        LogI("try forward request from [%s:%d] to [%s:%d], fd(%d)", src_ip.c_str(),
             _recv_ctx->from_port, forward_host->ip.c_str(), forward_host->port, _recv_ctx->fd)
        
        conn_to_webserver = MakeConnection(forward_host->ip, forward_host->port);
        if (!conn_to_webserver) {
            LogE("can NOT connect to web server [%s:%d]",
                 conn_to_webserver->RemoteIp().c_str(), conn_to_webserver->RemotePort())
            ReverseProxyServer::Instance().ReportWebServerDown(forward_host);
            continue;
        }
        break;
    }
    
    tcp::SendContext::Ptr send_ctx = conn_to_webserver->MakeSendContext();
    
    conn_map_[conn_to_webserver->Uid()] = std::make_pair(uid, _recv_ctx->return_packet);
    
    AutoBuffer *http_packet = GetConnection(uid)->TcpByteArray();
    send_ctx->buffer.ShallowCopyFrom(http_packet->Ptr(), http_packet->Length());
    
    TrySendAndMarkPendingIfUndone(send_ctx);
    
    return false;
}

bool ReverseProxyServer::NetThread::HandleHttpResponse(
                    const tcp::RecvContext::Ptr &_recv_ctx) {
    assert(_recv_ctx->type == tcp::kConnectTo);
    
    uint32_t webserver_uid = _recv_ctx->tcp_connection_uid;
    
    uint32_t client_uid = conn_map_[webserver_uid].first;
    tcp::ConnectionProfile *client_conn = GetConnection(client_uid);
    
    LogI("send back to client: [%s:%d]", client_conn->RemoteIp().c_str(),
         client_conn->RemotePort())

    tcp::SendContext::Ptr send_ctx = conn_map_[webserver_uid].second;
    
    AutoBuffer *http_packet = GetConnection(webserver_uid)->TcpByteArray();
    // Try shallow copy first.
    send_ctx->buffer.ShallowCopyFrom(http_packet->Ptr(), http_packet->Length());
    
    bool send_done = TrySendAndMarkPendingIfUndone(send_ctx);
    conn_map_.erase(client_uid);
    
    if (!send_done) {
        size_t nsend = send_ctx->buffer.Pos();
        send_ctx->buffer.Reset();
        // Deep copy the remaining part, because the connection
        // to the webserver will be deleted,
        // causing its TCP byte array to be deleted as well.
        send_ctx->buffer.Write(http_packet->Ptr(nsend),
                   http_packet->Length() - nsend);
    } else {
        DelConnection(client_uid);
    }
    DelConnection(webserver_uid);     // del connection to webserver.
    return true;
}

bool ReverseProxyServer::NetThread::HandleWebSocketPacket(
                    const tcp::RecvContext::Ptr &) {
    // TODO: WebSocket proxy.
    return false;
}

bool ReverseProxyServer::NetThread::HandleForwardFailed(
                const tcp::RecvContext::Ptr& _recv_ctx) {
    static std::string forward_failed_msg("Internal Server Error: Reverse Proxy Error");
    tcp::SendContext::Ptr return_packet = _recv_ctx->return_packet;
    http::response::Pack(http::THttpVersion::kHTTP_1_1, 500,
                         http::StatusLine::kStatusDescOk, nullptr,
                         return_packet->buffer, &forward_failed_msg);
    if (TrySendAndMarkPendingIfUndone(return_packet)) {
        DelConnection(_recv_ctx->tcp_connection_uid);
        return true;
    }
    return false;
}

ReverseProxyServer::NetThread::~NetThread() = default;

bool ReverseProxyServer::_CustomConfig(yaml::YamlDescriptor *_desc) {
    auto *conf = (ProxyConfig *) config_;
    
    try {
        std::vector<yaml::ValueObj *> & webservers_yml =
                _desc->GetArray(ProxyConfig::key_webservers)->AsYmlObjects();
    
        for (auto & webserver_yml : webservers_yml) {
            auto neo = new WebServerProfile();
            webserver_yml->GetLeaf(ProxyConfig::key_ip)->To(neo->ip);
            webserver_yml->GetLeaf(ProxyConfig::key_port)->To(neo->port);
            webserver_yml->GetLeaf(ProxyConfig::key_weight)->To(neo->weight);
            LogI("config webserver from yml: [%s:%d] weight: %d", neo->ip.c_str(),
                 neo->port, neo->weight)
            conf->webservers.push_back(neo);
        }
    
        _desc->GetLeaf(ProxyConfig::key_load_balance_rule)->To(conf->load_balance_rule);
        
        if (conf->load_balance_rule == ProxyConfig::kLoadBalanceRulePoll) {
            load_balancer_.ConfigRule(LoadBalancer::kPoll);
        } else if (conf->load_balance_rule == ProxyConfig::kLoadBalanceRuleWeight) {
            load_balancer_.ConfigRule(LoadBalancer::kWeight);
        } else if (conf->load_balance_rule == ProxyConfig::kLoadBalanceRuleIpHash) {
            load_balancer_.ConfigRule(LoadBalancer::kIpHash);
        } else {
            LogE("currently only the following load balancing policies "
                 "are supported: poll, weight, ip_hash.")
            return false;
        }
        LogI("load balance rule config to: %s", conf->load_balance_rule.c_str())
        
    } catch (std::exception &exception) {
        LogE("catch Yaml exception: %s", exception.what())
        return false;
    }
    return true;
}

const char *ReverseProxyServer::ConfigFile() {
    return kConfigFile;
}

bool ReverseProxyServer::CheckHeartbeat(const tcp::RecvContext::Ptr& _recv_ctx) {
    std::string &ip = _recv_ctx->from_ip;
    auto &webservers = ((ProxyConfig *) config_)->webservers;
    
    bool has_parsed = false;
    uint16_t port;
    uint32_t request_backlog;
    
    for (auto & webserver : webservers) {
        if (webserver->ip != ip) {
            continue;
        }
        if (!has_parsed) {
            auto http_req = std::dynamic_pointer_cast<http::request::HttpRequest>
                    (_recv_ctx->application_packet);
            AutoBuffer *body = http_req->Body();
            
            NetSceneSvrHeartbeatProto::NetSceneSvrHeartbeatReq req;
            req.ParseFromArray(body->Ptr(), (int) body->Length());
            if (!req.has_port()) {
                return false;
            }
            port = req.port();
            request_backlog = req.request_backlog();
            has_parsed = true;
        }
        if (webserver->port == port) {
            load_balancer_.OnRecvHeartbeat(webserver, request_backlog);
            LogI("heartbeat from [%s:%d], backlog: %u", ip.c_str(), port, request_backlog)
            return true;
        }
    }
    return false;
}

ReverseProxyServer::~ReverseProxyServer() = default;

