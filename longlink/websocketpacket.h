#pragma once
#include "http/headerfield.h"
#include "networkmodel/applicationlayer.h"


namespace ws {

void Pack(AutoBuffer &_content, AutoBuffer &_out);


class WebSocketPacket : public ApplicationPacket {
  public:
    WebSocketPacket();
    
    ~WebSocketPacket() override;
    
    bool DoHandShake();
    
    bool IsHandShaken() const;
    
    http::HeaderField &RequestHeaders();
    
    http::HeaderField &ResponseHeaders();
    
    TApplicationProtocol ApplicationProtocol() const override;
    
  private:
    static const char *const    kHandShakeMagicKey;
    bool                        is_hand_shaken;
    http::HeaderField           request_headers_;
    http::HeaderField           response_headers_;
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
 |     Extended payload length continued, if payload len == 127  |
 + - - - - - - - - - - - - - - - +-------------------------------+
 |8                              | Masking-key, if MASK set to 1 |
 +-------------------------------+-------------------------------+
 | Masking-key (continued)       |14       Payload Data          |
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
        kEnd,
        kError,
    };
    
    WebSocketParser(AutoBuffer *_buff,
                    WebSocketPacket *_packet,
                    http::HeaderField *_http_headers);
    
    ~WebSocketParser() override;
    
    int DoParse() override;
    
    bool IsErr() const override;
    
    bool IsEnd() const override;
    
  protected:

  private:
    WebSocketPacket           * ws_packet;
    TPosition                   position_;
    size_t                      resolved_len_;
};

}
