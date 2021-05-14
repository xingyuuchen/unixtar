#include "tcpconnection.h"
#include <cstring>
#include <utility>
#include "timeutil.h"
#include "socket/unixsocket.h"


namespace tcp {

const uint64_t ConnectionProfile::kDefaultTimeout = 60 * 1000;

ConnectionProfile::ConnectionProfile(std::string _remote_ip,
                                     uint16_t _remote_port, uint32_t _uid/* = 0*/)
        : uid_(_uid)
        , remote_ip_(std::move(_remote_ip))
        , remote_port_(_remote_port)
        , record_(::gettickcount())
        , timeout_millis_(kDefaultTimeout)
        , socket_(INVALID_SOCKET)
        , timeout_ts_(record_ + timeout_millis_)
        , application_packet_(nullptr)
        , application_protocol_parser_(nullptr) {
}


int ConnectionProfile::Receive() {
    
    SOCKET fd = socket_.FD();
    while (true) {
    
        bool has_more_data;
        ssize_t n = socket_.Recv(&tcp_byte_arr_, &has_more_data);
    
        if (socket_.IsEAgain()) {
            LogI("EAGAIN")
            return 0;
        }
        if (n < 0) {
            return -1;
        }
        if (n == 0) {
            // A read event is raised when conn closed by peer
            LogI("conn(%d) closed by peer", fd)
            return -1;
        }
    
        int ret = ParseProtocol();
        
        if (ret == -1) {
            LogE("No application_protocol_parser set")
            return -1;
        } else if (ret == -2) {
            return -1;
        }
    
        if (IsParseDone()) {
            MakeRecvContext();
            MakeSendContext();
            return 0;
        }
    
        if (!has_more_data) {
            return 0;
        }
    }
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

TApplicationProtocol ConnectionProfile::ApplicationProtocol() {
    if (!application_packet_) {
        return kUnknown;
    }
    return application_packet_->ApplicationProtocol();
}

const char *ConnectionProfile::ApplicationProtocolName() {
    if (application_packet_) {
        return ApplicationProtoToString[ApplicationProtocol()];
    }
    return nullptr;
}

bool ConnectionProfile::IsLongLinkApplicationProtocol() {
    if (!application_packet_) {
        return false;
    }
    return application_packet_->IsLongLink();
}

AutoBuffer *ConnectionProfile::TcpByteArray() { return &tcp_byte_arr_; }

void ConnectionProfile::CloseTcpConnection() { socket_.Close(); }

SOCKET ConnectionProfile::FD() const { return socket_.FD(); }

tcp::RecvContext *ConnectionProfile::GetRecvContext() { return &recv_ctx_; }

SendContext *ConnectionProfile::GetSendContext() { return &send_ctx_; }

uint64_t ConnectionProfile::GetTimeoutTs() const { return timeout_ts_; }

bool ConnectionProfile::IsTimeout(uint64_t _now) const {
    if (application_packet_ && application_packet_->IsLongLink()) {
        return false;
    }
    if (_now == 0) {
        _now = ::gettickcount();
    }
    return _now > timeout_ts_;
}

bool ConnectionProfile::IsTypeValid() const {
    TType type = GetType();
    return type == kTo || type == kFrom;
}

void ConnectionProfile::MakeRecvContext() {
}

void ConnectionProfile::MakeSendContext() {
    send_ctx_.connection_uid = Uid();
    send_ctx_.fd = socket_.FD();
}

std::string &ConnectionProfile::RemoteIp() { return remote_ip_; }

uint16_t ConnectionProfile::RemotePort() const { return remote_port_; }

ConnectionProfile::~ConnectionProfile() {
    delete application_protocol_parser_;
    application_protocol_parser_ = nullptr;
    delete application_packet_;
    application_packet_ = nullptr;
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
    }
    return ret;
}

ConnectionProfile::TType ConnectionTo::GetType() const { return kTo; }




ConnectionFrom::ConnectionFrom(SOCKET _fd, std::string _remote_ip,
                               uint16_t _remote_port, uint32_t _uid)
        : ConnectionProfile(std::move(_remote_ip), _remote_port, _uid) {
    
    socket_.Set(_fd);
    socket_.SetNonblocking();
}

ConnectionProfile::TType ConnectionFrom::GetType() const { return kFrom; }


void ConnectionFrom::MakeRecvContext() {
    if (!IsParseDone()) {
        LogE("recv ctx cannot be made when the parsing NOT done")
        return;
    }
    recv_ctx_.fd = socket_.FD();
    recv_ctx_.application_packet = application_packet_;
    recv_ctx_.send_context = &send_ctx_;
}

ConnectionFrom::~ConnectionFrom() = default;

}
