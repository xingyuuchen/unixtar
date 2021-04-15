#pragma once
#include "http/httprequest.h"
#include "socket/unix_socket.h"

namespace tcp {
struct SendContext {
    SOCKET          fd;
    AutoBuffer      buffer;
};
}

namespace http {
struct RecvContext {
    SOCKET              fd;
    tcp::SendContext *  send_context;
    bool                is_post;
    std::string         url_route;
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
    
    explicit ConnectionProfile(SOCKET _fd);
    
    ~ConnectionProfile();
    
    int Receive();
    
    /**
     *
     * Override these two if you use other application protocol
     * other than Http1_1. :)
     *
     * @return: 0 on success, non-0 on failure.
     */
    virtual int ParseProtocol();
    
    virtual bool IsParseDone();
    
    void CloseSelf();
    
    TApplicationProtocol GetApplicationProtocol() const;
    
    SOCKET FD() const;
    
    uint64_t GetTimeoutTs() const;
    
    bool IsTimeout(uint64_t _now = 0) const;
    
    http::request::Parser *GetHttpParser();
    
    http::RecvContext *GetRecvContext();
    
    SendContext *GetSendContext();


  private:
    void __MakeRecvContext();

  private:
    static const int        kBuffSize;
    static const uint64_t   kDefaultTimeout;
    SOCKET                  fd_;
    http::request::Parser   http_parser_;
    uint64_t                record_;
    uint64_t                timeout_millis_;
    uint64_t                timeout_ts_;
    TApplicationProtocol    application_protocol_;
    tcp::SendContext        send_ctx_{INVALID_SOCKET};
    http::RecvContext       recv_ctx_{INVALID_SOCKET, nullptr};
    
};

}
