#include "tcpconnection.h"
#include <cstring>
#include <cerrno>
#include <utility>
#include <unistd.h>
#include "timeutil.h"
#include "socket/unixsocket.h"


namespace tcp {

SendContext::SendContext()
        : tcp_connection_uid(0)
        , fd(INVALID_SOCKET)
        , is_longlink(false)
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
        , application_protocol_parser_(nullptr) {
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

void ConnectionProfile::AddPendingPacketToSend(SendContext::Ptr _send_ctx) {
    pending_send_ctx_.push(std::move(_send_ctx));
}

bool ConnectionProfile::TrySend(const SendContext::Ptr& _send_ctx) {
    if (!_send_ctx) {
        LogE("!_send_ctx")
        LogPrintStacktrace()
        return false;
    }
    AutoBuffer &resp = _send_ctx->buffer;
    size_t pos = resp.Pos();
    size_t ntotal = resp.Length() - pos;
    SOCKET fd = _send_ctx->fd;

    if (fd <= 0 || ntotal == 0) {
        return false;
    }

    ssize_t nwrite = ::write(fd, resp.Ptr(pos), ntotal);

    if (nwrite == ntotal) {
        LogI("fd(%d), write %zd/%zu B, done", fd, nwrite, ntotal)
        resp.Seek(AutoBuffer::kEnd);
        return true;
    }
    if (nwrite >= 0 || (nwrite < 0 && IS_EAGAIN(errno))) {
        nwrite = nwrite > 0 ? nwrite : 0;
        LogI("fd(%d): write %zd/%zu B", fd, nwrite, ntotal)
        resp.Seek(AutoBuffer::kCurrent, nwrite);
    }
    if (nwrite < 0) {
        if (errno == EPIPE) {
            // fd probably closed by peer, or cleared because of timeout.
            LogE("fd(%d) already closed, send nothing", fd)
            return false;
        }
        LogE("fd(%d) nwrite(%zd), errno(%d): %s",
             fd, nwrite, errno, strerror(errno))
        LogPrintStacktrace(5)
    }
    return false;
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

void ConnectionProfile::SendContextSendDoneCallback() {
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
    auto neo = std::make_shared<tcp::SendContext>();
    neo->tcp_connection_uid = Uid();
    neo->fd = socket_.FD();
    neo->is_longlink = IsLongLinkApplicationProtocol();
    neo->MarkAsPendingPacket = std::bind(
            &ConnectionProfile::AddPendingPacketToSend, this, neo);
    neo->OnSendDone = std::bind(
            &ConnectionProfile::SendContextSendDoneCallback, this);
    return neo;
}

std::string &ConnectionProfile::RemoteIp() { return remote_ip_; }

uint16_t ConnectionProfile::RemotePort() const { return remote_port_; }

ConnectionProfile::~ConnectionProfile() {
    if (!pending_send_ctx_.empty()) {
        LogE("pending_send_ctx_ NOT empty")
        LogPrintStacktrace()
        assert(false);
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
