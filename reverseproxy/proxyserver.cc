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
    if (_conn->GetType() == tcp::ConnectionProfile::kFrom) {
        _conn->ConfigApplicationLayer<http::request::HttpRequest,
                                      http::request::Parser>();
    } else {
        _conn->ConfigApplicationLayer<http::response::HttpResponse,
                                      http::response::Parser>();
    }
}

bool ReverseProxyServer::NetThread::HandleApplicationPacket(tcp::ConnectionProfile *_conn) {
    assert(_conn);
    if (_conn->ApplicationProtocol() != kHttp1_1) {
        LogE("%s NOT a Http1.1 packet", _conn->ApplicationProtocolName())
        return HandleForwardFailed(_conn);
    }
    
    tcp::ConnectionProfile::TType type = _conn->GetType();
    uint32_t uid = _conn->Uid();
    
    tcp::ConnectionProfile *to;
    
    if (type == tcp::ConnectionProfile::kFrom) {   // Forward to web server or heartbeat
    
        if (ReverseProxyServer::Instance().CheckHeartbeat(_conn)) {
            DelConnection(_conn->Uid());
            return true;
        }
    
        std::string src_ip = _conn->RemoteIp();
        tcp::ConnectionProfile *conn_to_webserver;
        
        while (true) {
            auto forward_host = ReverseProxyServer::Instance().LoadBalance(src_ip);
            if (!forward_host) {
                LogE("no web server to forward")
                return HandleForwardFailed(_conn);
            }
            LogI("try forward request from [%s:%d] to [%s:%d]", src_ip.c_str(),
                 _conn->RemotePort(), forward_host->ip.c_str(), forward_host->port)
    
            conn_to_webserver = MakeConnection(forward_host->ip, forward_host->port);
            if (!conn_to_webserver) {
                LogE("connection to web server [%s:%d] can NOT establish",
                        conn_to_webserver->RemoteIp().c_str(), conn_to_webserver->RemotePort())
                ReverseProxyServer::Instance().ReportWebServerDown(forward_host);
                continue;
            }
            break;
        }
    
        conn_to_webserver->MakeSendContext();
        
        conn_map_[conn_to_webserver->Uid()] = uid;
    
        to = conn_to_webserver;
        
    } else {    // Send back to client
        
        assert(type == tcp::ConnectionProfile::kTo);
        
        uint32_t client_uid = conn_map_[uid];
        to = GetConnection(client_uid);
        
        LogI("send back to client: [%s:%d]",
             to->RemoteIp().c_str(), to->RemotePort())
    }
    
    AutoBuffer *http_packet = _conn->TcpByteArray();
    AutoBuffer &buff = to->GetSendContext()->buffer;
    buff.ShallowCopyFrom(http_packet->Ptr(), http_packet->Length());
    
    bool send_done = TryWrite(to->GetSendContext());
    
    if (type == tcp::ConnectionProfile::kTo) {
        conn_map_.erase(to->Uid());
        DelConnection(uid);     // del connection to webserver.
        if (send_done) {
            DelConnection(to->GetSendContext()->connection_uid);
        }  // else { the epoll continue sending back. }
        return true;
    }
    return false;
}

bool ReverseProxyServer::NetThread::HandleForwardFailed(
                tcp::ConnectionProfile * _client_conn) {
    static std::string forward_failed_msg("Internal Server Error: Reverse Proxy Error");
    http::response::Pack(http::THttpVersion::kHTTP_1_1, 500,
                         http::StatusLine::kStatusDescOk, nullptr,
                         _client_conn->GetSendContext()->buffer, &forward_failed_msg);
    if (TryWrite(_client_conn->GetSendContext())) {
        DelConnection(_client_conn->Uid());
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

bool ReverseProxyServer::CheckHeartbeat(tcp::ConnectionProfile *_conn) {
    std::string &ip = _conn->RemoteIp();
    auto &webservers = ((ProxyConfig *) config_)->webservers;
    
    bool has_parsed = false;
    uint16_t port;
    uint32_t request_backlog;
    
    for (auto & webserver : webservers) {
        if (webserver->ip != ip) {
            continue;
        }
        if (!has_parsed) {
            auto http_req = (http::request::HttpRequest *) _conn->
                    GetRecvContext()->application_packet;
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
            load_balancer_.ReceiveHeartbeat(webserver, request_backlog);
            LogI("heartbeat from [%s:%d], backlog: %u", ip.c_str(), port, request_backlog)
            return true;
        }
    }
    return false;
}

ReverseProxyServer::~ReverseProxyServer() = default;

