#pragma once
#include <cassert>
#include <queue>
#include <list>
#include <memory>
#include <functional>
#include "socket/unixsocket.h"
#include "applicationlayer.h"
#include "log.h"


namespace tcp {

/* whether this connection is established actively or passively */
enum TConnectionType {
    kUnknown = 0,
    kAcceptFrom,
    kConnectTo,
};


struct SendContext {
    using Ptr = std::shared_ptr<tcp::SendContext>;
    explicit SendContext(uint32_t _seq);
    
    uint32_t                seq;
    uint32_t                tcp_connection_uid;
    Socket                * socket;
    AutoBuffer              buffer;
    bool                    is_tcp_conn_valid;
    std::function<void()>   MarkAsPendingPacket;
    std::function<void()>   OnSendDone;
};


struct RecvContext {
    using Ptr = std::shared_ptr<tcp::RecvContext>;
    RecvContext();
    /* <------ input fields begin ------> */
    uint32_t                            tcp_connection_uid;
    SOCKET                              fd;
    std::string                         from_ip;
    uint16_t                            from_port;
    TConnectionType                     type;
    ApplicationPacket::Ptr              application_packet;
    /* <------ input fields end ------> */
    
    /* <------ output fields begin ------> */
    std::vector<SendContext::Ptr>       packets_push_others;
    SendContext::Ptr                    return_packet;
    /* <------ output fields end ------> */
};
}


namespace tcp {
class ConnectionProfile {
  public:
    
    ConnectionProfile(std::string _remote_ip,
                      uint16_t _remote_port, uint32_t _uid = 0);
    
    virtual ~ConnectionProfile();
    
    int Receive();
    
    static bool TrySend(const SendContext::Ptr&);
    
    void AddPendingPacketToSend(SendContext::Ptr);
    
    bool HasPendingPacketToSend() const;
    
    bool TrySendPendingPackets();
    
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
        if (curr_application_packet_ && !IsUpgradeApplicationProtocol()) {
            LogE("application protocol already set && no need to upgrade")
            assert(false);
        }
        bool upgrade = IsUpgradeApplicationProtocol();
        
        ApplicationProtocolParser *old_parser = application_protocol_parser_;
        
        curr_application_packet_ = std::make_shared<ApplicationPacketImpl>();
        application_protocol_parser_ = new ApplicationParserImpl(&tcp_byte_arr_,
                std::dynamic_pointer_cast<ApplicationPacketImpl>(curr_application_packet_),
                _init_args...);
        application_protocol_ = curr_application_packet_->Protocol();
        is_longlink_app_proto_ = curr_application_packet_->IsLongLink();
        
        delete old_parser, old_parser = nullptr;
        LogI("app proto config to: %s", ApplicationProtocolName())
    
        if (upgrade) {
            tcp_byte_arr_.Reset();  // clear old tcp data.
        }
        if (curr_application_packet_->IsLongLink()) {
            socket_.SetTcpKeepAlive();   // enable Tcp heartbeat.
        }
    }
    
    bool IsUpgradeApplicationProtocol() const;
    
    TApplicationProtocol ProtocolUpgradeTo();
    
    TApplicationProtocol ApplicationProtocol();
    
    const char *ApplicationProtocolName();
    
    bool IsLongLinkApplicationProtocol() const;
    
    AutoBuffer *TcpByteArray();
    
    void CloseTcpConnection();
    
    Socket &GetSocket();
    
    SOCKET FD() const;
    
    bool IsTimeout(uint64_t _now = 0) const;
    
    void SendTcpFin() const;
    
    bool HasReceivedFin() const;
    
    void SendContextSendDoneCallback(const SendContext::Ptr&);
    
    virtual TConnectionType GetType() const = 0;
    
    bool IsTypeValid() const;
    
    RecvContext::Ptr MakeRecvContext(bool _with_send_ctx = false);

    SendContext::Ptr MakeSendContext();
    
    void DelSendContext(uint32_t _send_ctx_seq);
    
    std::string &RemoteIp();
    
    uint16_t RemotePort() const;

  protected:
    static const uint64_t               kDefaultTimeout;
    uint32_t                            uid_;
    TApplicationProtocol                application_protocol_;
    bool                                is_longlink_app_proto_;
    std::string                         remote_ip_;
    uint16_t                            remote_port_;
    Socket                              socket_;
    bool                                has_received_Fin_;
    uint64_t                            record_;
    uint64_t                            timeout_ts_;
    AutoBuffer                          tcp_byte_arr_;
    ApplicationPacket::Ptr              curr_application_packet_;
    ApplicationProtocolParser         * application_protocol_parser_;
    uint32_t                            send_ctx_seq_;
    std::list<SendContext::Ptr>         send_contexts_;
    std::queue<SendContext::Ptr>        pending_send_ctx_;
    
};


class ConnectionFrom : public ConnectionProfile {
  public:
    
    ConnectionFrom(SOCKET _fd, std::string _remote_ip,
                   uint16_t _remote_port, uint32_t _uid = 0);
    
    ~ConnectionFrom() override;
    
    TConnectionType GetType() const override;
    
  private:
  
};



class ConnectionTo : public ConnectionProfile {
  public:
    
    ConnectionTo(std::string _remote_ip,
                 uint16_t _remote_port, uint32_t _uid = 0);
    
    ~ConnectionTo() override;
    
    int Connect();
    
    TConnectionType GetType() const override;

  private:
  
};

}
