#include "websocketpacket.h"
#include <cstdint>


namespace ws {

void Pack(AutoBuffer &_content, AutoBuffer &_out) {
    uint8_t token = 0x81;
    size_t length = _content.Length();
//    if (length < 126) {
//        token += struct.pack("B", length)
//    } else if (length <= 0xFFFF) {
//        token += struct.pack("!BH", 126, length)  // !:bigendian
//    } else {
//        token += struct.pack("!BQ", 127, length)
//    }
//    ret = token + content
//    return ret
}



WebSocketPacket::WebSocketPacket()
        : ApplicationPacket() {
}

TApplicationProtocol WebSocketPacket::ApplicationProtocol() const {
    return kWebSocket;
}

WebSocketPacket::~WebSocketPacket() {

}



WebSocketParser::WebSocketParser( AutoBuffer *_buff, WebSocketPacket *_packet)
        : ApplicationProtocolParser(_packet, _buff) {
    
}

int WebSocketParser::DoParse() {
    
    return 0;
}

bool WebSocketParser::IsErr() const {
    return false;
}

bool WebSocketParser::IsEnd() const {
    return false;
}

bool WebSocketParser::_ResolveHandShake() {
//    data = str(self.__socket.recv(1024))
//
//    header_dict = {}
//    header, _ = data.split(r"\r\n\r\n", 1)
//    for line in header.split(r"\r\n")[1:]:
//    key, val = line.split(":", 1)
//    header_dict[key] = val
//
//    if "Sec-WebSocket-Key" not in header_dict:
//    self.__socket.close()
//    return False
//
//    std::string magic_key = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
//    sec_key = header_dict["Sec-WebSocket-Key"][1:] + magic_key  # charAt[0]=' '
//# print(sec_key)
//    key = base64.b64encode(hashlib.sha1(bytes(sec_key, encoding="utf-8")).digest())
//    key_str = str(key)[2: 30]
//# print(key_str)
//
//    response = "HTTP/1.1 101 Switching Protocols\r\n" \
//                   "Connection:Upgrade\r\n" \
//                   "Upgrade:websocket\r\n" \
//                   "Sec-WebSocket-Accept:{}\r\n" \
//                   "WebSocket-Protocol:chat\r\n\r\n".format(key_str)
//    self.__socket.send(bytes(response, encoding="utf-8"))
    return false;
}

WebSocketParser::~WebSocketParser() {

}
}