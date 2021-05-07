#include "proxyserver.h"
#include "log.h"


const char *const ReverseProxyServer::kConfigFile = "proxyserverconf.yml";

ReverseProxyServer::ReverseProxyServer()
        : HttpServer() {
    
}


LoadBalancer &ReverseProxyServer::GetLoadBalancer() { return load_balancer_; }


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

int ReverseProxyServer::NetThread::HandleHttpRequest(tcp::ConnectionProfile *_conn) {
    assert(_conn);
    
    tcp::ConnectionProfile::TType type = _conn->GetType();
    AutoBuffer *http_packet = _conn->GetHttpParser()->GetBuff();
    uint32_t uid = _conn->Uid();
    
    tcp::ConnectionProfile *to;
    bool is_delete;
    
    if (type == tcp::ConnectionProfile::kFrom) {   // Forward to web server
    
        auto selected = ReverseProxyServer::Instance().GetLoadBalancer().Select();
        
        std::string ip(selected->ip);
        LogI("forward request to [%s:%d]", ip.c_str(), selected->port)
        
        auto conn_to_webserver = MakeConnection(ip, selected->port);
    
        conn_to_webserver->MakeSendContext();
        
        conn_map_[conn_to_webserver->Uid()] = uid;
    
        to = conn_to_webserver;
        is_delete = false;
        
    } else {    // Send back to client
        
        assert(type == tcp::ConnectionProfile::kTo);
        
        uint32_t client_uid = conn_map_[uid];
        
        to = GetConnection(client_uid);
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

