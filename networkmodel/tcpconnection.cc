#include "tcpconnection.h"
#include <cstring>
#include <cerrno>
#include <utility>
#include <unistd.h>
#include "timeutil.h"
#include "socket/unixsocket.h"


namespace tcp {

SendContext::SendContext(uint32_t _seq)
        : seq(_seq)
        , tcp_connection_uid(0)
        , socket(nullptr)
        , is_tcp_conn_valid(true)
        , MarkAsPendingPacket(nullptr)
        , OnSendDone(nullptr) {
}

RecvContext::RecvContext()
        : fd(INVALID_SOCKET)
        , tcp_connection_uid(0)
        , from_port(0)
        , type(kUnknown)
        , application_packet(nullptr)
        , return_packet(nullptr) {
}


// For Connection from client, it will be considered timeout
// if no data is sent within such interval
// after the connection has been established.
const uint64_t ConnectionProfile::kDefaultTimeout = 10 * 1000;

ConnectionProfile::ConnectionProfile(std::string _remote_ip,
                                     uint16_t _remote_port, uint32_t _uid/* = 0*/)
        : uid_(_uid)
        , application_protocol_(kNone)
        , is_longlink_app_proto_(false)
        , remote_ip_(std::move(_remote_ip))
        , remote_port_(_remote_port)
        , record_(::gettickcount())
        , socket_(INVALID_SOCKET)
        , has_received_Fin_(false)
        , timeout_ts_(record_ + kDefaultTimeout)
        , curr_application_packet_(nullptr)
        , application_protocol_parser_(nullptr)
        , send_ctx_seq_(0) {
}


int ConnectionProfile::Receive() {
    
    SOCKET fd = socket_.FD();
    
    while (true) {
    
        bool has_more_data;
        ssize_t n = socket_.Receive(&tcp_byte_arr_, &has_more_data);
    
        if (socket_.IsEAgain()) {
            LogI("EAGAIN")
            return 0;
        }
        if (n < 0) {
            return -1;
        }
        if (n == 0) {   // FIN.
            has_received_Fin_ = true;
            LogI("fd(%d), uid: %d, peer sent FIN", fd, uid_)
            return 0;
        }
    
        int ret = ParseProtocol();
        
        if (ret == -1) {
            LogE("No application_protocol_parser set")
            return -1;
        } else if (ret == -2) {
            return -1;
        }
    
        if (IsParseDone() || !has_more_data) {
            return 0;
        }
    }
}

void ConnectionProfile::AddPendingPacketToSend(
            SendContext::Ptr _send_ctx) {
    pending_send_ctx_.push(std::move(_send_ctx));
}

bool ConnectionProfile::TrySend(const SendContext::Ptr& _send_ctx) {
    if (!_send_ctx) {
        LogE("!_send_ctx")
        LogPrintStacktrace()
        return false;
    }
    AutoBuffer &resp = _send_ctx->buffer;
    bool is_send_done = false;

    _send_ctx->socket->Send(&resp, &is_send_done);

    return is_send_done;
}

bool ConnectionProfile::HasPendingPacketToSend() const {
    return !pending_send_ctx_.empty();
}

bool ConnectionProfile::TrySendPendingPackets() {
    LogD("fd(%d), %lu pending packets waiting to be sent",
                FD(), pending_send_ctx_.size())
    while (!pending_send_ctx_.empty()) {
        auto send_ctx = pending_send_ctx_.front();
        bool is_send_done = TrySend(send_ctx);
        if (is_send_done) {
            if (send_ctx->OnSendDone) {
                send_ctx->OnSendDone();
            }
            pending_send_ctx_.pop();
            continue;
        }
        return false;
    }
    return true;
}

uint32_t ConnectionProfile::Uid() const { return uid_; }

void ConnectionProfile::SetUid(uint32_t _uid) { uid_ = _uid; }

int ConnectionProfile::ParseProtocol() {
    if (!application_protocol_parser_) {
        LogE("please config application protocol first")
        return -1;
    }
    
    application_protocol_parser_->DoParse();
    
    if (application_protocol_parser_->IsErr()) {
        LogI("parser error")
        return -2;
    }
    return 0;
}


bool ConnectionProfile::IsParseDone() {
    assert(application_protocol_parser_);
    return application_protocol_parser_->IsEnd();
}

bool ConnectionProfile::IsUpgradeApplicationProtocol() const {
    if (!application_protocol_parser_) {
        return false;
    }
    return application_protocol_parser_->IsUpgradeProtocol();
}

TApplicationProtocol ConnectionProfile::ProtocolUpgradeTo() {
    if (!IsUpgradeApplicationProtocol()) {
        return TApplicationProtocol::kNone;
    }
    return application_protocol_parser_->ProtocolUpgradeTo();
}

TApplicationProtocol ConnectionProfile::ApplicationProtocol() {
    return application_protocol_;
}

const char *ConnectionProfile::ApplicationProtocolName() {
    return ApplicationProtoToString[ApplicationProtocol()];
}

bool ConnectionProfile::IsLongLinkApplicationProtocol() const {
    return is_longlink_app_proto_;
}

AutoBuffer *ConnectionProfile::TcpByteArray() { return &tcp_byte_arr_; }

void ConnectionProfile::CloseTcpConnection() { socket_.Close(); }

Socket &ConnectionProfile::GetSocket() { return socket_; }

SOCKET ConnectionProfile::FD() const { return socket_.FD(); }

bool ConnectionProfile::IsTimeout(uint64_t _now) const {
    if (IsLongLinkApplicationProtocol()) {
        return false;
    }
    if (!pending_send_ctx_.empty()) {
        // timeout just because of poor networking
        // is not considered as timeout.
        return false;
    }
    if (_now == 0) {
        _now = ::gettickcount();
    }
    return _now > timeout_ts_;
}

void ConnectionProfile::SendTcpFin() const {
    socket_.ShutDown(SHUT_WR);
}

bool ConnectionProfile::HasReceivedFin() const { return has_received_Fin_; }

void ConnectionProfile::SendContextSendDoneCallback(
                const SendContext::Ptr& _send_ctx) {
    if (!IsLongLinkApplicationProtocol()) {
        // After sending the return packet, the client is expected
        // to send a Tcp Fin within the specific interval,
        // else such connection will be considered as timeout.
        timeout_ts_ = ::gettickcount() + kDefaultTimeout;
        if (GetType() == kConnectTo) {
            timeout_ts_ += kDefaultTimeout;
        }
    }
    DelSendContext(_send_ctx->seq);
}

bool ConnectionProfile::IsTypeValid() const {
    TConnectionType type = GetType();
    return type == kAcceptFrom || type == kConnectTo;
}

RecvContext::Ptr ConnectionProfile::MakeRecvContext(
                        bool _with_send_ctx /* = false */) {
    auto neo = std::make_shared<tcp::RecvContext>();
    neo->fd = socket_.FD();
    neo->tcp_connection_uid = Uid();
    neo->from_ip = std::string(RemoteIp());
    neo->from_port = RemotePort();
    neo->type = GetType();
    neo->application_packet = curr_application_packet_;
    if (IsLongLinkApplicationProtocol()) {
        // Resets parser to clear data of last application packet,
        // because longlink protocol reuses the parser.
        application_protocol_parser_->Reset();
        
        // Allocates a new packet because the old one probably has not
        // been processed by the worker thread when new data comes and
        // a new application packet is needed to be parsed in.
        curr_application_packet_ = curr_application_packet_->AllocNewPacket();
        application_protocol_parser_->SetPacketToParse(curr_application_packet_);
    }
    neo->return_packet = _with_send_ctx ? MakeSendContext() : nullptr;
    return neo;
}

SendContext::Ptr ConnectionProfile::MakeSendContext() {
    auto neo = std::make_shared<tcp::SendContext>(++send_ctx_seq_);
    neo->tcp_connection_uid = Uid();
    neo->socket = &socket_;
    neo->is_tcp_conn_valid = true;
    neo->MarkAsPendingPacket = std::bind(
            &ConnectionProfile::AddPendingPacketToSend, this, neo);
    neo->OnSendDone = std::bind(
            &ConnectionProfile::SendContextSendDoneCallback, this, neo);
    send_contexts_.push_back(neo);
    return neo;
}

void ConnectionProfile::DelSendContext(uint32_t _send_ctx_seq) {
    send_contexts_.remove_if([=] (SendContext::Ptr &ptr) -> bool {
        return ptr->seq == _send_ctx_seq;
    });
}

std::string &ConnectionProfile::RemoteIp() { return remote_ip_; }

uint16_t ConnectionProfile::RemotePort() const { return remote_port_; }

ConnectionProfile::~ConnectionProfile() {
    if (!pending_send_ctx_.empty()) {
        LogI("pending_send_ctx_ NOT empty, deleted probably because of FIN")
    }
    // Notify all send_ctx not to send anymore.
    for (auto & send_ctx : send_contexts_) {
        send_ctx->is_tcp_conn_valid = false;
    }
    delete application_protocol_parser_;
    application_protocol_parser_ = nullptr;
}



ConnectionTo::ConnectionTo(std::string _remote_ip, uint16_t _remote_port, uint32_t _uid)
        : ConnectionProfile(std::move(_remote_ip),_remote_port, _uid) {
    
    if (socket_.Create(AF_INET, SOCK_STREAM)) {
        LogE("Create socket_ failed")
    }
}

ConnectionTo::~ConnectionTo() = default;

int ConnectionTo::Connect() {
    int ret = socket_.Connect(remote_ip_, remote_port_);
    if (ret == 0) {
        socket_.SetNonblocking();
        socket_.SetTcpNoDelay();    // disable Nagle's algorithm
    }
    return ret;
}

TConnectionType ConnectionTo::GetType() const { return kConnectTo; }




ConnectionFrom::ConnectionFrom(SOCKET _fd, std::string _remote_ip,
                               uint16_t _remote_port, uint32_t _uid)
        : ConnectionProfile(std::move(_remote_ip), _remote_port, _uid) {
    
    assert(_fd > 0);
    socket_.Set(_fd);
    socket_.SetConnected(true);
    socket_.SetNonblocking();
    socket_.SetTcpNoDelay();    // disable Nagle's algorithm
}

TConnectionType ConnectionFrom::GetType() const { return kAcceptFrom; }


ConnectionFrom::~ConnectionFrom() = default;

}
