#include "websocketpacket.h"
#include <cassert>
#include "log.h"
#include "messagedigest.h"
#include "base64.h"


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


const char *const WebSocketPacket::kHandShakeMagicKey =
                "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

WebSocketPacket::WebSocketPacket()
        : ApplicationPacket()
        , is_hand_shaken(false) {
}

TApplicationProtocol WebSocketPacket::ApplicationProtocol() const { return kWebSocket; }

bool WebSocketPacket::IsHandShaken() const { return is_hand_shaken; }

bool WebSocketPacket::DoHandShake() {
    assert(!is_hand_shaken);
    response_headers_.Reset();
    const char *sec_ws_key = request_headers_.Get(http::HeaderField::kSecWebSocketKey);
    if (!sec_ws_key) {
        return false;
    }
    
    std::string sec_key(sec_ws_key);
    sec_key.append(kHandShakeMagicKey);
    
    unsigned char sha1[21] = {0, };
    md::Sha1::Digest((const unsigned char *) sec_key.data(), sec_key.size(), sha1);
    
    char base64[256];
    base64::Encode(sha1, 20, base64);
    
    response_headers_.InsertOrUpdate(http::HeaderField::kSecWebSocketAccept,
                                     std::string(base64));
    response_headers_.InsertOrUpdate(http::HeaderField::kConnection,
                                     http::HeaderField::kUpgrade);
    response_headers_.InsertOrUpdate(http::HeaderField::kUpgrade,
                                     http::HeaderField::kWebSocket);
    is_hand_shaken = true;
    LogI("sec_key: %s, Sec-WebSocket-Accept: %s", sec_key.c_str(), base64)
    return is_hand_shaken;
}

http::HeaderField &WebSocketPacket::RequestHeaders() { return request_headers_; }

http::HeaderField &WebSocketPacket::ResponseHeaders() { return response_headers_; }

WebSocketPacket::~WebSocketPacket() {

}



WebSocketParser::WebSocketParser(AutoBuffer *_buff,
                                 WebSocketPacket *_packet,
                                 http::HeaderField *_http_headers)
        : ApplicationProtocolParser(_packet, _buff)
        , ws_packet(_packet)
        , position_(kNone)
        , resolved_len_(0) {
    
}

int WebSocketParser::DoParse() {
    assert(ws_packet->IsHandShaken());
    if (buffer_->Length() < 0) {
        LogE("")
        return -1;
    }
    size_t unresolved_len = buffer_->Length() - resolved_len_;
    if (unresolved_len <= 0) {
        LogI("no bytes need to be resolved: %zd", unresolved_len)
        return 0;
    }
    
    std::string data(buffer_->Ptr(), buffer_->Length());
    LogI("data: %s", data.c_str())
    
    size_t payload_len = 0;
    
    return 0;
}

bool WebSocketParser::IsErr() const {
    return false;
}

bool WebSocketParser::IsEnd() const {
    return true;
}

WebSocketParser::~WebSocketParser() = default;

}

