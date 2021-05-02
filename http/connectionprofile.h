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
    
    enum TApplicationProtocol {
        kUnknown = 0,
        kHttp1_1,   // default
        kHttp2_0,
    };
    
    ConnectionProfile(uint32_t _uid, SOCKET _fd, std::string _src_ip,
                      uint16_t _src_port);
    
    ~ConnectionProfile();
    
    int Receive();
    
    uint32_t Uid() const;
    
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
    
    std::string &GetSrcIp();
    
    uint16_t GetPort() const;
    
    bool IsTimeout(uint64_t _now = 0) const;
    
    http::request::Parser *GetHttpParser();
    
    http::RecvContext *GetRecvContext();
    
    SendContext *GetSendContext();


  private:
    void __MakeRecvContext();

  private:
    static const uint64_t   kDefaultTimeout;
    uint32_t                uid_;
    std::string             src_ip_;
    uint16_t                src_port_;
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
