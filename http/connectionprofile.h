#pragma once
#include "http/httprequest.h"
#include "http/httpresponse.h"
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
    
    ConnectionProfile(std::string _remote_ip,
                      uint16_t _remote_port, uint32_t _uid = 0);
    
    virtual ~ConnectionProfile();
    
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
    int ParseProtocol();
    
    virtual bool IsParseDone();
    
    virtual http::ParserBase *GetParser() = 0;
    
    void CloseTcpConnection();
    
    TApplicationProtocol GetApplicationProtocol() const;
    
    SOCKET FD() const;
    
    uint64_t GetTimeoutTs() const;

    bool IsTimeout(uint64_t _now = 0) const;
    
    virtual TType GetType() const = 0;
    
    bool IsTypeValid() const;
    
    http::RecvContext *GetRecvContext();
    
    SendContext *GetSendContext();

    virtual void MakeRecvContext();
    
    void MakeSendContext();
    
    std::string &RemoteIp();
    
    uint16_t RemotePort() const;

  private:
    virtual AutoBuffer *RecvBuff() = 0;

  protected:
    static const uint64_t   kDefaultTimeout;
    uint32_t                uid_;
    std::string             remote_ip_;
    uint16_t                remote_port_;
    Socket                  socket_;
    uint64_t                record_;
    uint64_t                timeout_millis_;
    uint64_t                timeout_ts_;
    TApplicationProtocol    application_protocol_;
    tcp::SendContext        send_ctx_{0, INVALID_SOCKET};
    http::RecvContext       recv_ctx_{INVALID_SOCKET, nullptr};
    
};


class ConnectionFrom : public ConnectionProfile {
  public:
    
    ConnectionFrom(SOCKET _fd, std::string _remote_ip,
                   uint16_t _remote_port, uint32_t _uid = 0);
    
    ~ConnectionFrom() override;
    
    bool IsParseDone() override;
    
    TType GetType() const override;
    
    void MakeRecvContext() override;
    http::ParserBase *GetParser() override;

  private:
    AutoBuffer *RecvBuff() override;

  private:
    http::request::Parser   http_req_parser_;

};



class ConnectionTo : public ConnectionProfile {
  public:
    
    ConnectionTo(std::string _remote_ip,
                 uint16_t _remote_port, uint32_t _uid = 0);
    
    ~ConnectionTo() override;
    
    int Connect();
    
    bool IsParseDone() override;
    
    TType GetType() const override;

    http::ParserBase *GetParser() override;

  private:
    AutoBuffer *RecvBuff() override;

  private:
    http::response::Parser  http_resp_parser_;
};

}
