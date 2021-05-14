#pragma once
#include "socket/unixsocket.h"
#include "applicationpacket.h"
#include "log.h"


namespace tcp {
struct SendContext {
    uint32_t        connection_uid;
    SOCKET          fd;
    AutoBuffer      buffer;
};
}

namespace http {
struct RecvContext {
    SOCKET                  fd;
    tcp::SendContext      * send_context;
    ApplicationPacket     * application_packet;
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
    
    bool IsParseDone();
    
    template<class ApplicationPacketImpl, class ApplicationParserImpl>
    void
    ConfigApplicationLayer() {
        if (application_packet_ && !application_packet_->IsLongLink()) {
            LogE("application protocol already configured.")
            assert(false);
        }
        delete application_packet_, application_packet_ = nullptr;
        delete application_protocol_parser_, application_protocol_parser_ = nullptr;
        
        application_packet_ = new ApplicationPacketImpl();
        application_protocol_parser_ = new ApplicationParserImpl(
                &tcp_byte_arr_, (ApplicationPacketImpl *) application_packet_);
        LogI("application protocol config to: %d",
             application_packet_->ApplicationProtocol())
    }
    
    virtual bool IsLongLink();
    
    AutoBuffer *TcpByteArray();
    
    void CloseTcpConnection();
    
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

  protected:
    static const uint64_t   kDefaultTimeout;
    uint32_t                uid_;
    std::string             remote_ip_;
    uint16_t                remote_port_;
    Socket                  socket_;
    uint64_t                record_;
    uint64_t                timeout_millis_;
    uint64_t                timeout_ts_;
    AutoBuffer              tcp_byte_arr_;
    
    ApplicationPacket     * application_packet_;
    ApplicationProtocolParser     * application_protocol_parser_;
    
    tcp::SendContext        send_ctx_{0, INVALID_SOCKET};
    http::RecvContext       recv_ctx_{INVALID_SOCKET, nullptr};
    
};


class ConnectionFrom : public ConnectionProfile {
  public:
    
    ConnectionFrom(SOCKET _fd, std::string _remote_ip,
                   uint16_t _remote_port, uint32_t _uid = 0);
    
    ~ConnectionFrom() override;
    
    TType GetType() const override;
    
    void MakeRecvContext() override;
    
  private:
  
};



class ConnectionTo : public ConnectionProfile {
  public:
    
    ConnectionTo(std::string _remote_ip,
                 uint16_t _remote_port, uint32_t _uid = 0);
    
    ~ConnectionTo() override;
    
    int Connect();
    
    TType GetType() const override;

  private:
  
};

}
