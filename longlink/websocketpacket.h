#pragma once
#include "http/headerfield.h"
#include "networkmodel/applicationlayer.h"


namespace ws {

void Pack(std::string &_content, AutoBuffer &_out);


class WebSocketPacket : public ApplicationPacket {
  public:
    using Ptr = std::shared_ptr<WebSocketPacket>;
    
    static const char *const    kHandShakeMagicKey;
    static const uint8_t        kOpcodeText;
    static const uint8_t        kOpcodeConnectionClose;
    
    static const uint16_t       kStatusCodeInvalid;
    static const uint16_t       kStatusCodeCloseNormal;
    static const uint16_t       kStatusCodeCloseGoingAway;
    static const uint16_t       kStatusCodeCloseProtocolError;
    static const uint16_t       kStatusCodeCloseUnsupported;
    static const uint16_t       kStatusCodeCloseNoStatus;
    static const uint16_t       kStatusCodeCloseAbnormal;
    static const uint16_t       kStatusCodeUnsupportedData;
    static const uint16_t       kStatusCodePolicyViolation;
    static const uint16_t       kStatusCodeCloseTooLarge;
    static const uint16_t       kStatusCodeMissingExtension;
    static const uint16_t       kStatusCodeInternalError;
    static const uint16_t       kStatusCodeServiceRestart;
    static const uint16_t       kStatusCodeTryAgainLater;
    static const uint16_t       kStatusCodeTlsHandShake;
    
    static const char *const    kStatusCodeInvalidInfo;
    static const char *const    kStatusCodeCloseNormalInfo;
    static const char *const    kStatusCodeCloseGoingAwayInfo;
    static const char *const    kStatusCodeCloseProtocolErrorInfo;
    static const char *const    kStatusCodeCloseUnsupportedInfo;
    static const char *const    kStatusCodeCloseNoStatusInfo;
    static const char *const    kStatusCodeCloseAbnormalInfo;
    static const char *const    kStatusCodeUnsupportedDataInfo;
    static const char *const    kStatusCodePolicyViolationInfo;
    static const char *const    kStatusCodeCloseTooLargeInfo;
    static const char *const    kStatusCodeMissingExtensionInfo;
    static const char *const    kStatusCodeInternalErrorInfo;
    static const char *const    kStatusCodeServiceRestartInfo;
    static const char *const    kStatusCodeTryAgainLaterInfo;
    static const char *const    kStatusCodeTlsHandShakeInfo;
    
    WebSocketPacket();
    
    ~WebSocketPacket() override;
    
    bool DoHandShake();
    
    bool IsHandShaken() const;
    
    void SetHandShakeReqHeader(http::HeaderField *);
    
    http::HeaderField &RequestHeaders();
    
    http::HeaderField &ResponseHeaders();
    
    void SetFirstByte(uint8_t _first);
    
    uint8_t OpCode() const;
    
    uint16_t StatusCode();
    
    const char *StatusCodeInfo();
    
    void Masked(bool _mask);
    
    bool IsMasked() const;
    
    void SetPayloadLen(uint8_t _len);
    
    uint8_t PayloadLen() const;
    
    void SetExtendedPayloadLen(size_t _extended_payload_len);
    
    size_t ExtendedPayloadLen() const;
    
    void SetMaskingKey(uint8_t[4]);
    
    uint8_t *MaskingKey();
    
    std::string &Payload();
    
    void Reset();
    
    TApplicationProtocol Protocol() const override;
    
    ApplicationPacket::Ptr AllocNewPacket() override;
    
  private:
    bool                        is_hand_shaken_;
    http::HeaderField           handshake_req_;
    http::HeaderField           handshake_resp_;
    uint8_t                     first_byte_;
    bool                        fin_;
    bool                        rsv_[3]{false};
    uint8_t                     op_code_;
    bool                        mask_;
    uint8_t                     payload_len_;
    size_t                      extended_payload_len_;
    uint8_t                     masking_key_[4]{};
    std::string                 payload_;
};


/**
                         WebSocket 包格式
0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +-+-+-+-+-------+-+-------------+-------------------------------+
 |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
 |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
 |N|V|V|V|       |S|             |   (if payload len==126/127)   |
 | |1|2|3|       |K|             |                               |
 +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
 |    Extended payload length continued, if payload len == 127   |
 + - - - - - - - - - - - - - - - +-------------------------------+
 |8                              | Masking-key, if MASK set to 1 |
 +-------------------------------+-------------------------------+
 |    Masking-key (continued)    |14       Payload Data          |
 +-------------------------------- - - - - - - - - - - - - - - - +
 :                     Payload Data continued ...                :
 + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
 |                     Payload Data continued ...                |
 +---------------------------------------------------------------+

具体每一bit:
    FIN      1bit 表示信息的最后一帧
    RSV 1-3  1bit each 以后备用的 默认都为 0
    Opcode   4bit 帧类型，稍后细说
    Mask     1bit 掩码，是否加密数据，默认必须置为1
    Payload len  7bit 数据的长度
    Masking-key      1 or 4 bit 掩码
    Payload data     (x + y) bytes 数据
    Extension data   x bytes  扩展数据
    Application data y bytes  程序数据
 */

class WebSocketParser : public ApplicationProtocolParser {
  public:
    enum TPosition {
        kNone = 0,
        kFirstByte,
        kPayloadLen,
        kExtendedPayloadLen,
        kMaskingKey,
        kPayload,
        kEnd,
        kError,
    };
    
    WebSocketParser(AutoBuffer *_buff,
                    const WebSocketPacket::Ptr& _packet,
                    http::HeaderField *_handshake_req);
    
    ~WebSocketParser() override;
    
    int DoParse() override;
    
    bool IsErr() const override;
    
    bool IsEnd() const override;
    
    void Reset() override;
    
    void OnApplicationPacketChanged(
            const ApplicationPacket::Ptr&) override;

  protected:
    bool _ResolveFirstByte();
    
    bool _ResolvePayloadLen();
    
    bool _ResolveExtendedPayloadLen();
    
    bool _ResolveMaskingKey();
    
    bool _ResolvePayload();

  private:
    WebSocketPacket::Ptr        ws_packet_;
    TPosition                   position_;
    size_t                      resolved_len_;
};

}
