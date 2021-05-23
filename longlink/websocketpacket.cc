#include "websocketpacket.h"
#include <cassert>
#include "log.h"
#include "messagedigest.h"
#include "base64.h"


namespace ws {

void Pack(std::string &_content, AutoBuffer &_out) {
    uint8_t token = 0x80 + WebSocketPacket::kOpcodeText;
    _out.Write((char *) &token, 1);
    
    size_t length = _content.size();
    if (length < 126) {
        uint8_t len = length;
        _out.Write((char *) &len, 1);
        
    } else if (length <= 0xffff) {     // bigendian
        uint8_t n126 = 126;
        uint16_t len = length;
        _out.Write((char *) &n126, 1);
        _out.Write((char *) &len, 2);
        
    } else {     // bigendian
        uint8_t n127 = 127;
        uint64_t len = length;
        _out.Write((char *) &n127, 1);
        _out.Write((char *) &len, 8);
    }
    _out.Write(_content.data(), _content.size());
}


const char *const WebSocketPacket::kHandShakeMagicKey =
                "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
const uint8_t WebSocketPacket::kOpcodeText                      = 0b0001;
const uint8_t WebSocketPacket::kOpcodeConnectionClose           = 0b1000;

const uint16_t WebSocketPacket::kStatusCodeInvalid              = 0000;
const uint16_t WebSocketPacket::kStatusCodeCloseNormal          = 1000;
const uint16_t WebSocketPacket::kStatusCodeCloseGoingAway       = 1001;
const uint16_t WebSocketPacket::kStatusCodeCloseProtocolError   = 1002;
const uint16_t WebSocketPacket::kStatusCodeCloseUnsupported     = 1003;
const uint16_t WebSocketPacket::kStatusCodeCloseNoStatus        = 1005;
const uint16_t WebSocketPacket::kStatusCodeCloseAbnormal        = 1006;
const uint16_t WebSocketPacket::kStatusCodeUnsupportedData      = 1007;
const uint16_t WebSocketPacket::kStatusCodePolicyViolation      = 1008;
const uint16_t WebSocketPacket::kStatusCodeCloseTooLarge        = 1009;
const uint16_t WebSocketPacket::kStatusCodeMissingExtension     = 1010;
const uint16_t WebSocketPacket::kStatusCodeInternalError        = 1011;
const uint16_t WebSocketPacket::kStatusCodeServiceRestart       = 1012;
const uint16_t WebSocketPacket::kStatusCodeTryAgainLater        = 1013;
const uint16_t WebSocketPacket::kStatusCodeTlsHandShake         = 1015;

const char *const WebSocketPacket::kStatusCodeInvalidInfo = "Invalid Status Code";
const char *const WebSocketPacket::kStatusCodeCloseNormalInfo = "Close Normal";
const char *const WebSocketPacket::kStatusCodeCloseGoingAwayInfo = "Close Going Away";
const char *const WebSocketPacket::kStatusCodeCloseProtocolErrorInfo = "Close Protocol Error";
const char *const WebSocketPacket::kStatusCodeCloseUnsupportedInfo = "Close Unsupported";
const char *const WebSocketPacket::kStatusCodeCloseNoStatusInfo = "Close No Status";
const char *const WebSocketPacket::kStatusCodeCloseAbnormalInfo = "Close Abnormal";
const char *const WebSocketPacket::kStatusCodeUnsupportedDataInfo = "Unsupported Data";
const char *const WebSocketPacket::kStatusCodePolicyViolationInfo = "Policy Violation";
const char *const WebSocketPacket::kStatusCodeCloseTooLargeInfo = "Close Too Large";
const char *const WebSocketPacket::kStatusCodeMissingExtensionInfo = "Missing Extension";
const char *const WebSocketPacket::kStatusCodeInternalErrorInfo = "Internal Error";
const char *const WebSocketPacket::kStatusCodeServiceRestartInfo = "Service Restart";
const char *const WebSocketPacket::kStatusCodeTryAgainLaterInfo = "TryAgain Later";
const char *const WebSocketPacket::kStatusCodeTlsHandShakeInfo = "TlsHand Shake";


WebSocketPacket::WebSocketPacket()
        : ApplicationPacket()
        , is_hand_shaken(false)
        , first_byte(0x81)
        , fin_(false)
        , op_code_(0)
        , mask_(false)
        , payload_len_(0)
        , extended_payload_len_(0) {
}

WebSocketPacket::~WebSocketPacket() = default;

TApplicationProtocol WebSocketPacket::Protocol() const { return kWebSocket; }

bool WebSocketPacket::IsHandShaken() const { return is_hand_shaken; }

bool WebSocketPacket::DoHandShake() {
    assert(!is_hand_shaken);
    handshake_resp.Reset();
    const char *sec_ws_key = handshake_req.Get(http::HeaderField::kSecWebSocketKey);
    if (!sec_ws_key) {
        LogE("request do Not have header field: %s", http::HeaderField::kSecWebSocketKey)
        return false;
    }
    
    std::string sec_key(sec_ws_key);
    sec_key.append(kHandShakeMagicKey);
    
    unsigned char sha1[21] = {0, };
    md::Sha1::Digest((const unsigned char *) sec_key.data(), sec_key.size(), sha1);
    
    char base64[256] = {0, };
    base64::Encode(sha1, 20, base64);
    
    handshake_resp.InsertOrUpdate(http::HeaderField::kSecWebSocketAccept,
                                     std::string(base64));
    handshake_resp.InsertOrUpdate(http::HeaderField::kConnection,
                                     http::HeaderField::kUpgrade);
    handshake_resp.InsertOrUpdate(http::HeaderField::kUpgrade,
                                     http::HeaderField::kWebSocket);
    is_hand_shaken = true;
    LogI("sec_key: %s, Sec-WebSocket-Accept: %s", sec_key.c_str(), base64)
    return is_hand_shaken;
}

void WebSocketPacket::SetHandShakeReqHeader(http::HeaderField *_header) {
    handshake_req = *_header;
}

http::HeaderField &WebSocketPacket::RequestHeaders() { return handshake_req; }

http::HeaderField &WebSocketPacket::ResponseHeaders() { return handshake_resp; }

void WebSocketPacket::SetFirstByte(uint8_t _first) {
    first_byte = _first;
    fin_ = first_byte & 0x80;
    rsv_[0] = first_byte & 0x40;
    rsv_[1] = first_byte & 0x20;
    rsv_[2] = first_byte & 0x10;
    op_code_ = first_byte & 0x0f;
}

uint8_t WebSocketPacket::OpCode() const { return op_code_; }

uint16_t WebSocketPacket::StatusCode() {
    if (payload_.size() < 2) {
        return kStatusCodeInvalid;
    }
    return ((payload_.at(0) << 8) & 0xff00) + (payload_.at(1) & 0xff);
}

const char *WebSocketPacket::StatusCodeInfo() {
    uint16_t status_code = StatusCode();
    if (status_code == kStatusCodeInvalid)
        return kStatusCodeInvalidInfo;
    if (status_code == kStatusCodeCloseNormal)
        return kStatusCodeCloseNormalInfo;
    if (status_code == kStatusCodeCloseGoingAway)
        return kStatusCodeCloseGoingAwayInfo;
    if (status_code == kStatusCodeCloseProtocolError)
        return kStatusCodeCloseProtocolErrorInfo;
    if (status_code == kStatusCodeCloseUnsupported)
        return kStatusCodeCloseUnsupportedInfo;
    if (status_code == kStatusCodeCloseNoStatus)
        return kStatusCodeCloseNoStatusInfo;
    if (status_code == kStatusCodeCloseAbnormal)
        return kStatusCodeCloseAbnormalInfo;
    if (status_code == kStatusCodeUnsupportedData)
        return kStatusCodeUnsupportedDataInfo;
    if (status_code == kStatusCodePolicyViolation)
        return kStatusCodePolicyViolationInfo;
    if (status_code == kStatusCodeCloseTooLarge)
        return kStatusCodeCloseTooLargeInfo;
    if (status_code == kStatusCodeMissingExtension)
        return kStatusCodeMissingExtensionInfo;
    if (status_code == kStatusCodeInternalError)
        return kStatusCodeInternalErrorInfo;
    if (status_code == kStatusCodeServiceRestart)
        return kStatusCodeServiceRestartInfo;
    if (status_code == kStatusCodeTryAgainLater)
        return kStatusCodeTryAgainLaterInfo;
    if (status_code == kStatusCodeTlsHandShake)
        return kStatusCodeTlsHandShakeInfo;
    LogE("unknown status code: %u", status_code)
    return nullptr;
}

void WebSocketPacket::Masked(bool _mask) { mask_ = _mask; }

bool WebSocketPacket::IsMasked() const { return mask_; }

void WebSocketPacket::SetPayloadLen(uint8_t _len) { payload_len_ = _len; }

uint8_t WebSocketPacket::PayloadLen() const { return payload_len_; }

void WebSocketPacket::SetExtendedPayloadLen(size_t _extended_payload_len) {
    extended_payload_len_ = _extended_payload_len;
}

size_t WebSocketPacket::ExtendedPayloadLen() const { return extended_payload_len_; }

void WebSocketPacket::SetMaskingKey(uint8_t _masking_key[4]) {
    memcpy(masking_key_, _masking_key, 4);
}

uint8_t *WebSocketPacket::MaskingKey() { return masking_key_; }

std::string &WebSocketPacket::Payload() { return payload_; }

void WebSocketPacket::Reset() {
    first_byte = 0;
    mask_ = false;
    payload_len_ = 0;
    extended_payload_len_ = 0;
    memset(masking_key_, 0, 4);
    payload_.clear();
}

ApplicationPacket::Ptr WebSocketPacket::AllocNewPacket() {
    auto neo = std::make_shared<WebSocketPacket>();
    neo->is_hand_shaken = true;
    return neo;
}


WebSocketParser::WebSocketParser(AutoBuffer *_buff,
                                 const WebSocketPacket::Ptr& _packet,
                                 http::HeaderField *_handshake_req)
        : ApplicationProtocolParser(_packet, _buff)
        , ws_packet(nullptr)
        , position_(kNone)
        , resolved_len_(0) {
    
    ws_packet = std::dynamic_pointer_cast<ws::WebSocketPacket>(application_packet_);
    ws_packet->SetHandShakeReqHeader(_handshake_req);
}

int WebSocketParser::DoParse() {
    assert(ws_packet->IsHandShaken());
    if (buffer_->Length() < 0) {
        LogE("wtf")
        return -1;
    }
    size_t unresolved_len = buffer_->Length() - resolved_len_;
    if (unresolved_len <= 0) {
        LogI("no bytes need to be resolved: %zd", unresolved_len)
        return 0;
    }
    
    while (true) {
        bool success = false;
        if (position_ == kNone) {
            resolved_len_ = 0;
            position_ = kFirstByte;
            success = _ResolveFirstByte();
            
        } else if (position_ == kFirstByte) {
            success = _ResolveFirstByte();
            
        } else if (position_ == kPayloadLen) {
            success = _ResolvePayloadLen();
            
        } else if (position_ == kExtendedPayloadLen) {
            success = _ResolveExtendedPayloadLen();
            
        } else if (position_ == kMaskingKey) {
            success = _ResolveMaskingKey();
            
        } else if (position_ == kPayload) {
            success = _ResolvePayload();
        }
    
        if (position_ == kEnd) {
            return 0;
        }
        if (position_ == kError) {
            LogI("error occurred, do nothing")
            return -1;
        }
        if (!success) {
            return 0;
        }
    }
}

bool WebSocketParser::IsErr() const { return position_ == kError; }

bool WebSocketParser::IsEnd() const { return position_ == kEnd; }

void WebSocketParser::OnApplicationPacketChanged(
        const ApplicationPacket::Ptr& _neo) {
    ws_packet = std::dynamic_pointer_cast<ws::WebSocketPacket>(_neo);
}

bool WebSocketParser::_ResolveFirstByte() {
    ws_packet->SetFirstByte(*buffer_->Ptr(0));
    resolved_len_ += 1;
    position_ = kPayloadLen;
    return true;
}

bool WebSocketParser::_ResolvePayloadLen() {
    if (buffer_->Length() - resolved_len_ < 1) {
        return false;
    }
    bool is_mask = (*buffer_->Ptr(1)) & 0x80;
    ws_packet->Masked(is_mask);
    
    uint8_t payload_len = (*buffer_->Ptr(1)) & 0x7f;
    LogI("mask: %d, payload len: %hhu", is_mask, payload_len)
    
    if (payload_len == 126 || payload_len == 127) {
        position_ = kExtendedPayloadLen;
    } else if (payload_len < 126) {
        position_ = is_mask ? kMaskingKey : kPayload;
    } else {
        position_ = kError;
        return false;
    }
    ws_packet->SetPayloadLen(payload_len);
    resolved_len_ += 1;
    return true;
}

bool WebSocketParser::_ResolveExtendedPayloadLen() {
    size_t unresolved_len = buffer_->Length() - resolved_len_;
    
    uint8_t payload_len = ws_packet->PayloadLen();
    
    if (payload_len == 126) {
        if (unresolved_len < 2) {
            return false;
        }
        uint16_t extended_payload_len = ((*buffer_->Ptr(resolved_len_) << 8) & 0xff00) +
                (*buffer_->Ptr(resolved_len_ + 1) & 0xff);
        resolved_len_ += 2;
        ws_packet->SetExtendedPayloadLen(extended_payload_len);
        
    } else if (payload_len == 127) {
        if (unresolved_len < 8) {
            return false;
        }
        size_t extended_payload_len = 0;
        for (int i = 0; i < 8; ++i) {
            int shift = (7 - i) * 8;
            uint8_t c = *buffer_->Ptr(resolved_len_++);
            extended_payload_len += (c << shift) & (0xff << shift);
        }
        ws_packet->SetExtendedPayloadLen(extended_payload_len);
        
    } else {
        position_ = kError;
        return false;
    }
    position_ = ws_packet->IsMasked() ? kMaskingKey : kPayload;
    return true;
}

bool WebSocketParser::_ResolveMaskingKey() {
    if (buffer_->Length() - resolved_len_ < 4) {
        return false;
    }
    if (ws_packet->IsMasked()) {
        uint8_t mask_key[4];
        memcpy(mask_key, buffer_->Ptr(resolved_len_), sizeof(mask_key));
        LogI("mask-key: %02x %02x %02x %02x", mask_key[0],
                    mask_key[1], mask_key[2], mask_key[3])
        ws_packet->SetMaskingKey(mask_key);
        resolved_len_ += 4;
        position_ = kPayload;
    }
    return true;
}

bool WebSocketParser::_ResolvePayload() {
    size_t unresolved_len = buffer_->Length() - resolved_len_;
    if (unresolved_len <= 0) {
        return false;
    }
    size_t payload_len = ws_packet->PayloadLen();
    std::string &payload = ws_packet->Payload();
    
    bool mask = ws_packet->IsMasked();
    uint8_t *masking_key = ws_packet->MaskingKey();
    
    for (int i = 0; i < unresolved_len; ++i) {
        uint8_t raw = *buffer_->Ptr(resolved_len_ + i);
        uint8_t decoded = mask ? raw ^ masking_key[i % 4] :raw;
        payload += (char) decoded;
    }
    
    resolved_len_ += unresolved_len;
    if (payload.size() == payload_len) {
        position_ = kEnd;
        return true;
    } else if (payload.size() > payload_len) {
        position_ = kError;
    }
    return false;
}

void WebSocketParser::Reset() {
    LogI("reset ws parser")
    position_ = kNone;
    resolved_len_ = 0;
    buffer_->Reset();
}

WebSocketParser::~WebSocketParser() = default;

}

