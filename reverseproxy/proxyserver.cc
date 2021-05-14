#include "proxyserver.h"
#include "log.h"
#include "http/httpresponse.h"


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
    assert(_conn->GetType() == tcp::ConnectionProfile::kFrom);
    _conn->ConfigApplicationLayer<http::response::HttpResponse,
            http::response::Parser>();
}

int ReverseProxyServer::NetThread::HandleApplicationPacket(tcp::ConnectionProfile *_conn) {
    assert(_conn);
    if (_conn->ApplicationProtocol() != kHttp1_1) {
        LogE("%s NOT a Http1.1 packet", _conn->ApplicationProtocolName())
        HandleForwardFailed(_conn);
        return -1;
    }
    
    tcp::ConnectionProfile::TType type = _conn->GetType();
    uint32_t uid = _conn->Uid();
    
    tcp::ConnectionProfile *to;
    
    if (type == tcp::ConnectionProfile::kFrom) {   // Forward to web server
    
        std::string src_ip = _conn->RemoteIp();
        
        tcp::ConnectionProfile *conn_to_webserver;
        while (true) {
            auto forward_host = ReverseProxyServer::Instance().LoadBalance(src_ip);
            if (!forward_host) {
                LogE("no web server to forward")
                HandleForwardFailed(_conn);
                return -1;
            }
            LogI("try forward request from [%s:%d] to [%s:%d]", src_ip.c_str(),
                 _conn->RemotePort(), forward_host->ip.c_str(), forward_host->port)
    
            conn_to_webserver = MakeConnection(forward_host->ip, forward_host->port);
            if (!conn_to_webserver) {
                LoadBalancer::ReportWebServerDown(forward_host);
                LogE("connection to web server [%s:%d] can NOT establish",
                        conn_to_webserver->RemoteIp().c_str(), conn_to_webserver->RemotePort())
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
    
    bool send_done = _OnWriteEvent(to->GetSendContext());
    
    if (type == tcp::ConnectionProfile::kTo) {
        conn_map_.erase(to->Uid());
        DelConnection(uid);
        if (send_done) {
            DelConnection(to->GetSendContext()->connection_uid);
        }
    }
    return 0;
}

void ReverseProxyServer::NetThread::HandleForwardFailed(
                tcp::ConnectionProfile * _client_conn) {
    static std::string forward_failed_msg("Internal Server Error: Reverse Proxy Error");
    std::string status_desc("OK");
    http::response::Pack(http::THttpVersion::kHTTP_1_1, 200,
                         status_desc, nullptr,
                         _client_conn->GetSendContext()->buffer, forward_failed_msg);
    if (_OnWriteEvent(_client_conn->GetSendContext())) {
        DelConnection(_client_conn->Uid());
    }
}

ReverseProxyServer::NetThread::~NetThread() = default;

bool ReverseProxyServer::_CustomConfig(yaml::YamlDescriptor *_desc) {
    auto *conf = (ProxyConfig *) config_;
    
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
    return true;
}

const char *ReverseProxyServer::ConfigFile() {
    return kConfigFile;
}

ReverseProxyServer::~ReverseProxyServer() = default;

