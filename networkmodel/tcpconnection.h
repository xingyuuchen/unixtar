#pragma once
#include <cassert>
#include "socket/unixsocket.h"
#include "applicationlayer.h"
#include "log.h"


namespace tcp {
struct SendContext {
    SendContext();
    uint32_t        connection_uid;
    SOCKET          fd;
    bool            is_longlink;
    AutoBuffer      buffer;
};


struct RecvContext {
    RecvContext();
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
     * @return: 0 on success, non-0 on failure.
     */
    int ParseProtocol();
    
    bool IsParseDone();
    
    
    template<class ApplicationPacketImpl /* : public ApplicationPacket */,
             class ApplicationParserImpl /* : public ApplicationProtocolParser */,
             class ...Args>
    void
    ConfigApplicationLayer(Args &&..._init_args) {
        if (application_packet_ && !IsUpgradeApplicationProtocol()) {
            LogE("application protocol already set, and no need to upgrade")
            assert(false);
        }
        bool upgrade = IsUpgradeApplicationProtocol();
        
        ApplicationPacket *old_packet = application_packet_;
        ApplicationProtocolParser *old_parser = application_protocol_parser_;
        
        application_packet_ = new ApplicationPacketImpl();
        application_protocol_parser_ = new ApplicationParserImpl(&tcp_byte_arr_,
                                (ApplicationPacketImpl *) application_packet_,
                                _init_args...);
        delete old_packet;
        delete old_parser;
        LogI("application protocol config to: %s", ApplicationProtocolName())
    
        if (upgrade) {  // update context.
            MakeRecvContext();
            MakeSendContext();
            tcp_byte_arr_.Reset();  // clear old tcp data.
        }
    }
    
    bool IsUpgradeApplicationProtocol() const;
    
    TApplicationProtocol ProtocolUpgradeTo();
    
    TApplicationProtocol ApplicationProtocol();
    
    const char *ApplicationProtocolName();
    
    bool IsLongLinkApplicationProtocol() const;
    
    AutoBuffer *TcpByteArray();
    
    void CloseTcpConnection();
    
    SOCKET FD() const;
    
    uint64_t GetTimeoutTs() const;

    bool IsTimeout(uint64_t _now = 0) const;
    
    bool HasReceivedFIN() const;
    
    virtual TType GetType() const = 0;
    
    bool IsTypeValid() const;
    
    tcp::RecvContext *GetRecvContext();
    
    tcp::SendContext *GetSendContext();

    virtual void MakeRecvContext();
    
    void MakeSendContext();
    
    std::string &RemoteIp();
    
    uint16_t RemotePort() const;

  protected:
    static const uint64_t       kDefaultTimeout;
    uint32_t                    uid_;
    std::string                 remote_ip_;
    uint16_t                    remote_port_;
    Socket                      socket_;
    bool                        has_received_FIN_;
    uint64_t                    record_;
    uint64_t                    timeout_millis_;
    uint64_t                    timeout_ts_;
    AutoBuffer                  tcp_byte_arr_;
    ApplicationPacket         * application_packet_;
    ApplicationProtocolParser * application_protocol_parser_;
    tcp::SendContext            send_ctx_;
    tcp::RecvContext            recv_ctx_;
    
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
