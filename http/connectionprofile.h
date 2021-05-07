#pragma once
#include "http/httprequest.h"
#include "socket/unixsocket.h"

namespace tcp {
struct SendContext {
    uint32_t        connection_uid;
    SOCKET          fd;
    AutoBuffer      buffer;
};
}

namespace http {
struct RecvContext {
    SOCKET              fd;
    tcp::SendContext *  send_context;
    bool                is_post;
    std::string         full_url;
    AutoBuffer          http_body;
};
}


namespace tcp {
class ConnectionProfile {
  public:
    /* whether this connection is established actively or passively */
    enum TType {
        kFrom = 0,
        kTo,
    };
    
    enum TApplicationProtocol {
        kUnknown = 0,
        kHttp1_1,   // default
        kHttp2_0,
        kHttp3_0,
    };
    
    /* for kFrom */
    ConnectionProfile(uint32_t _uid, SOCKET _fd, std::string _src_ip,
                      uint16_t _src_port);
    
    /* for kTo */
    ConnectionProfile(uint32_t _uid, std::string _dst_ip,
                      uint16_t _dst_port);
    
    ~ConnectionProfile();
    
    int Connect();
    
    int Receive();
    
    uint32_t Uid() const;
    
    void SetUid(uint32_t _uid);
    
    /**
     *
     * Override these two if you use other application protocol
     * other than Http1_1. :)
     *
     * @return: 0 on success, non-0 on failure.
     */
    virtual int ParseProtocol();
    
    virtual bool IsParseDone();
    
    void CloseTcpConnection();
    
    TApplicationProtocol GetApplicationProtocol() const;
    
    SOCKET FD() const;
    
    uint64_t GetTimeoutTs() const;
    
    std::string &SrcIp();
    
    std::string &DstIp();
    
    uint16_t SrcPort() const;
    
    uint16_t DstPort() const;
    
    bool IsTimeout(uint64_t _now = 0) const;
    
    TType GetType() const;
    
    bool IsTypeValid() const;
    
    http::request::Parser *GetHttpParser();
    
    void MakeContext();
    
    void MakeSendContext();
    
    http::RecvContext *GetRecvContext();
    
    SendContext *GetSendContext();

    
  private:
    TType                   type_;
    static const uint64_t   kDefaultTimeout;
    uint32_t                uid_;
    std::string             src_ip_;
    std::string             dst_ip_;
    uint16_t                src_port_;
    uint16_t                dst_port_;
    Socket                  socket_;
    http::request::Parser   http_parser_;
    uint64_t                record_;
    uint64_t                timeout_millis_;
    uint64_t                timeout_ts_;
    TApplicationProtocol    application_protocol_;
    tcp::SendContext        send_ctx_{0, INVALID_SOCKET};
    http::RecvContext       recv_ctx_{INVALID_SOCKET, nullptr};
    
};

}
