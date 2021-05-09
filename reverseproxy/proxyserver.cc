#include "proxyserver.h"
#include "log.h"


const char *const ReverseProxyServer::kConfigFile = "proxyserverconf.yml";

ReverseProxyServer::ReverseProxyServer()
        : HttpServer() {
    
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


ReverseProxyServer::NetThread::NetThread()
        : HttpNetThread() {
}

void ReverseProxyServer::AfterConfig() {
    
    SetNetThreadImpl<NetThread>();
    
}

int ReverseProxyServer::NetThread::HandleHttpPacket(tcp::ConnectionProfile *_conn) {
    assert(_conn);
    
    tcp::ConnectionProfile::TType type = _conn->GetType();
    AutoBuffer *http_packet;
    uint32_t uid = _conn->Uid();
    
    tcp::ConnectionProfile *to;
    
    if (type == tcp::ConnectionProfile::kFrom) {   // Forward to web server
    
        std::string src_ip = _conn->RemoteIp();
        
        auto forward_host = ReverseProxyServer::Instance().LoadBalance(src_ip);
        
        LogI("forward request from [%s:%d] to [%s:%d]", src_ip.c_str(),
             _conn->RemotePort(), forward_host->ip.c_str(), forward_host->port)
        
        auto conn_to_webserver = MakeConnection(forward_host->ip, forward_host->port);
        if (!conn_to_webserver) {
            LogE("connection to web server can NOT establish")
            HandleForwardFailed(_conn);
            return -1;
        }
    
        conn_to_webserver->MakeSendContext();
        
        conn_map_[conn_to_webserver->Uid()] = uid;
    
        http_packet = _conn->GetParser()->GetBuff();
        to = conn_to_webserver;
        
    } else {    // Send back to client
        
        assert(type == tcp::ConnectionProfile::kTo);
        
        uint32_t client_uid = conn_map_[uid];
        to = GetConnection(client_uid);
        
        http_packet = _conn->GetParser()->GetBuff();
        LogI("send back to client: [%s:%d]",
             to->RemoteIp().c_str(), to->RemotePort())
        
    }
    
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
    std::string forward_failed_msg("Internal Server Error: Reverse Proxy Error");
    std::string status_desc("OK");
    std::map<std::string, std::string> headers;
    http::response::Pack(http::THttpVersion::kHTTP_1_1, 200,
                         status_desc, headers,
                         _client_conn->GetSendContext()->buffer, forward_failed_msg);
    if (_OnWriteEvent(_client_conn->GetSendContext())) {
        DelConnection(_client_conn->Uid());
    }
}

ReverseProxyServer::NetThread::~NetThread() = default;

bool ReverseProxyServer::_CustomConfig(yaml::YamlDescriptor &_desc) {
    
    return true;
}

const char *ReverseProxyServer::ConfigFile() {
    return kConfigFile;
}

ReverseProxyServer::~ReverseProxyServer() = default;

