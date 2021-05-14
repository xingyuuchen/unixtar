#include "websocketserver.h"



const char *const WebSocketServer::kConfigFile = "longlinkserverconf.yml";

WebSocketServer::~WebSocketServer() {

}

void WebSocketServer::BeforeConfig() {

}

void WebSocketServer::AfterConfig() {

}

const char *WebSocketServer::ConfigFile() {
    return kConfigFile;
}

ServerBase::ServerConfigBase *WebSocketServer::_MakeConfig() {
    return ServerBase::_MakeConfig();
}

bool WebSocketServer::_CustomConfig(yaml::YamlDescriptor *_desc) {
    return true;
}



WebSocketServer::NetThread::~NetThread() {

}

void WebSocketServer::NetThread::OnStart() {

}

void WebSocketServer::NetThread::OnStop() {

}

int WebSocketServer::NetThread::HandShake(tcp::ConnectionProfile *_conn) {
    if (_conn->Receive() < 0) {
        DelConnection(_conn->Uid());
    }
    return 0;
}

int WebSocketServer::NetThread::_OnReadEvent(tcp::ConnectionProfile *_conn) {
    return 0;
}

bool WebSocketServer::NetThread::_OnWriteEvent(tcp::SendContext *_send_ctx) {
    return false;
}
