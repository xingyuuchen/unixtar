#include "websocketserver.h"
#include "websocketpacket.h"



const char *const WebSocketServer::kConfigFile = "longlinkserverconf.yml";

WebSocketServer::WebSocketServer()
        : ServerBase() {
    
}

WebSocketServer::~WebSocketServer() = default;

void WebSocketServer::BeforeConfig() {

}

void WebSocketServer::AfterConfig() {

}

const char *WebSocketServer::ConfigFile() { return kConfigFile; }

ServerBase::ServerConfigBase *WebSocketServer::_MakeConfig() {
    return ServerBase::_MakeConfig();
}

bool WebSocketServer::_CustomConfig(yaml::YamlDescriptor *_desc) {
    return true;
}



WebSocketServer::NetThread::NetThread()
        : NetThreadBase() {
}

bool WebSocketServer::NetThread::HandleApplicationPacket(
                tcp::ConnectionProfile *_conn) {
    if (_conn->ApplicationProtocol() != kWebSocket) {
        LogE("%s Not a WebSocket packet", _conn->ApplicationProtocolName())
        DelConnection(_conn->Uid());
        return true;
    }
    // TODO
    return false;
}

void WebSocketServer::NetThread::OnStart() {

}

void WebSocketServer::NetThread::OnStop() {

}

void WebSocketServer::NetThread::ConfigApplicationLayer(
                tcp::ConnectionProfile *_conn) {
    _conn->ConfigApplicationLayer<ws::WebSocketPacket,
                                  ws::WebSocketParser>();
}

int WebSocketServer::NetThread::HandShake(tcp::ConnectionProfile *_conn) {
    if (_conn->Receive() < 0) {
        DelConnection(_conn->Uid());
    }
    return 0;
}

WebSocketServer::NetThread::~NetThread() = default;

