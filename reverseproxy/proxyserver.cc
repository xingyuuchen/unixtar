#include "proxyserver.h"
#include "log.h"
#include "http/httprequest.h"
#include "http/httpresponse.h"
#include "netscenesvrheartbeat.pb.h"


const char *const ReverseProxyServer::kConfigFile = "proxyserverconf.yml";
const char *const ReverseProxyServer::ProxyConfig::key_ip = "ip";
const char *const ReverseProxyServer::ProxyConfig::key_port = "port";
const char *const ReverseProxyServer::ProxyConfig::key_weight = "weight";
const char *const ReverseProxyServer::ProxyConfig::key_webservers = "webservers";


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

void ReverseProxyServer::NetThread::ConfigApplicationLayer(tcp::ConnectionProfile *_conn) {
    if (_conn->GetType() == tcp::TConnectionType::kAcceptFrom) {
        _conn->ConfigApplicationLayer<http::request::HttpRequest,
                                      http::request::Parser>();
    } else {
        _conn->ConfigApplicationLayer<http::response::HttpResponse,
                                      http::response::Parser>();
    }
}

bool ReverseProxyServer::NetThread::HandleApplicationPacket(tcp::RecvContext::Ptr _recv_ctx) {
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

bool ReverseProxyServer::NetThread::HandleHttpPacket(const tcp::RecvContext::Ptr& _recv_ctx) {
    tcp::TConnectionType type = _recv_ctx->type;
    uint32_t uid = _recv_ctx->tcp_connection_uid;
    
    tcp::SendContext::Ptr send_ctx;
    
    if (type == tcp::TConnectionType::kAcceptFrom) {  // Forward to web server / Handle heartbeat
    
        if (ReverseProxyServer::Instance().CheckHeartbeat(_recv_ctx)) {
            DelConnection(_recv_ctx->tcp_connection_uid);
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
                LogE("connection to web server [%s:%d] can NOT establish",
                        conn_to_webserver->RemoteIp().c_str(), conn_to_webserver->RemotePort())
                ReverseProxyServer::Instance().ReportWebServerDown(forward_host);
                continue;
            }
            break;
        }
    
        send_ctx = conn_to_webserver->MakeSendContext();
        
        conn_map_[conn_to_webserver->Uid()] = std::make_pair(uid, _recv_ctx->packet_back);
        
    } else {    // Send back to client
        
        assert(type == tcp::TConnectionType::kConnectTo);
        
        uint32_t client_uid = conn_map_[uid].first;
        tcp::ConnectionProfile *client_conn = GetConnection(client_uid);
        send_ctx = conn_map_[uid].second;
        
        LogI("send back to client: [%s:%d]", client_conn->RemoteIp().c_str(),
                            client_conn->RemotePort())
    }
    
    AutoBuffer *http_packet = GetConnection(uid)->TcpByteArray();
    // Try shallow copy first.
    send_ctx->buffer.ShallowCopyFrom(http_packet->Ptr(), http_packet->Length());
    
    bool send_done = TrySendAndMarkPendingIfUndone(send_ctx);
    
    if (type == tcp::TConnectionType::kConnectTo) {
        uint32_t client_uid = send_ctx->tcp_connection_uid;
        conn_map_.erase(client_uid);
        if (send_done) {
            DelConnection(client_uid);
        } else {
            size_t nsend = send_ctx->buffer.Pos();
            send_ctx->buffer.Reset();
            // Deep copy the remaining part, because the connection
            // to the webserver will be deleted,
            // causing its TCP byte array to be deleted as well.
            send_ctx->buffer.Write(http_packet->Ptr(nsend),
                   http_packet->Length() - nsend);
        }
        DelConnection(uid);     // del connection to webserver.
        return true;
    }
    return false;
}

bool ReverseProxyServer::NetThread::HandleWebSocketPacket(const tcp::RecvContext::Ptr &) {
    // TODO: WebSocket proxy.
    return false;
}

bool ReverseProxyServer::NetThread::HandleForwardFailed(
                const tcp::RecvContext::Ptr& _recv_ctx) {
    static std::string forward_failed_msg("Internal Server Error: Reverse Proxy Error");
    tcp::SendContext::Ptr send_ctx = _recv_ctx->packet_back;
    http::response::Pack(http::THttpVersion::kHTTP_1_1, 500,
                         http::StatusLine::kStatusDescOk, nullptr,
                         send_ctx->buffer, &forward_failed_msg);
    if (TrySendAndMarkPendingIfUndone(send_ctx)) {
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

