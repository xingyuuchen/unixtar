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
    bool is_delete;
    
    if (type == tcp::ConnectionProfile::kFrom) {   // Forward to web server
    
        std::string src_ip = _conn->RemoteIp();
        
        auto forward = ReverseProxyServer::Instance().LoadBalance(src_ip);
        
        LogI("forward request from [%s:%d] to [%s:%d]", src_ip.c_str(),
             _conn->RemotePort(), forward->ip.c_str(), forward->port)
        
        auto conn_to_webserver = MakeConnection(forward->ip, forward->port);
    
        conn_to_webserver->MakeSendContext();
        
        conn_map_[conn_to_webserver->Uid()] = uid;
    
        http_packet = _conn->GetParser()->GetBuff();
        to = conn_to_webserver;
        is_delete = false;
        
    } else {    // Send back to client
        
        assert(type == tcp::ConnectionProfile::kTo);
        
        uint32_t client_uid = conn_map_[uid];
        to = GetConnection(client_uid);
        
        http_packet = _conn->GetParser()->GetBuff();
        LogI("send back to client: [%s:%d]",
             to->RemoteIp().c_str(), to->RemotePort())
        is_delete = true;
    }
    
    AutoBuffer &buff = to->GetSendContext()->buffer;
    
    buff.SetPtr(http_packet->Ptr());
    buff.SetLength(http_packet->Length());
    buff.ShallowCopy(true);
    
    _OnWriteEvent(to->GetSendContext(), is_delete);
    
    if (is_delete) {
        conn_map_.erase(to->Uid());
        DelConnection(uid);
    }
    return 0;
}

ReverseProxyServer::NetThread::~NetThread() = default;

bool ReverseProxyServer::_CustomConfig(yaml::YamlDescriptor &_desc) {
    
    return true;
}

const char *ReverseProxyServer::ConfigFile() {
    return kConfigFile;
}

ReverseProxyServer::~ReverseProxyServer() = default;

