#include "connectionprofile.h"
#include <cstring>
#include <utility>
#include "timeutil.h"
#include "log.h"
#include "socket/unixsocket.h"


namespace tcp {

const uint64_t ConnectionProfile::kDefaultTimeout = 60 * 1000;

ConnectionProfile::ConnectionProfile(std::string _remote_ip,
                                     uint16_t _remote_port, uint32_t _uid/* = 0*/)
        : uid_(_uid)
        , remote_ip_(std::move(_remote_ip))
        , remote_port_(_remote_port)
        , application_protocol_(TApplicationProtocol::kHttp1_1)
        , record_(::gettickcount())
        , timeout_millis_(kDefaultTimeout)
        , socket_(INVALID_SOCKET)
        , timeout_ts_(record_ + timeout_millis_) {
}


int ConnectionProfile::Receive() {
    
    AutoBuffer *recv_buff = RecvBuff();
    
    SOCKET fd = socket_.FD();
    while (true) {
    
        bool has_more_data;
        ssize_t n = socket_.Recv(recv_buff, &has_more_data);
    
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
            LogE("Please override func: ParseProtocol "
                           "when using other protocol than Http1.1")
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
    if (application_protocol_ != TApplicationProtocol::kHttp1_1) {
        // Override me if you use other application protocol
        // other than Http1_1. :)
        return -1;
    }
    
    http::ParserBase *parser = GetParser();
    
    parser->DoParse();
    
    if (parser->IsErr()) {
        LogI("parser error")
        return -2;
    }
    return 0;
}


bool ConnectionProfile::IsParseDone() {
    if (application_protocol_ != TApplicationProtocol::kHttp1_1) {
        return false;
    }
    return true;
}

void ConnectionProfile::CloseTcpConnection() { socket_.Close(); }

ConnectionProfile::TApplicationProtocol ConnectionProfile::GetApplicationProtocol() const {
    return application_protocol_;
}

SOCKET ConnectionProfile::FD() const { return socket_.FD(); }

http::RecvContext *ConnectionProfile::GetRecvContext() { return &recv_ctx_; }

SendContext *ConnectionProfile::GetSendContext() { return &send_ctx_; }

uint64_t ConnectionProfile::GetTimeoutTs() const { return timeout_ts_; }

bool ConnectionProfile::IsTimeout(uint64_t _now) const {
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

ConnectionProfile::~ConnectionProfile() = default;





ConnectionTo::ConnectionTo(std::string _dst_ip, uint16_t _dst_port, uint32_t _uid)
        : ConnectionProfile(std::move(_dst_ip),_dst_port, _uid) {
    
    if (socket_.Create(AF_INET, SOCK_STREAM)) {
        LogE("Create socket_ failed")
    }
    socket_.SetNonblocking();
}

ConnectionTo::~ConnectionTo() = default;

int ConnectionTo::Connect() {
    return socket_.Connect(remote_ip_, remote_port_);
}

AutoBuffer *ConnectionTo::RecvBuff() { return http_resp_parser_.GetBuff(); }

bool ConnectionTo::IsParseDone() {
    if (!ConnectionProfile::IsParseDone()) {
        return false;
    }
    return http_resp_parser_.IsEnd();
}

ConnectionProfile::TType ConnectionTo::GetType() const { return kTo; }

http::ParserBase *ConnectionTo::GetParser() { return &http_resp_parser_; }




ConnectionFrom::ConnectionFrom(SOCKET _fd, std::string _remote_ip,
                               uint16_t _remote_port, uint32_t _uid)
        : ConnectionProfile(std::move(_remote_ip), _remote_port, _uid) {
    
    socket_.Set(_fd);
    socket_.SetNonblocking();
}

ConnectionProfile::TType ConnectionFrom::GetType() const { return kFrom; }

AutoBuffer *ConnectionFrom::RecvBuff() { return http_req_parser_.GetBuff(); }

bool ConnectionFrom::IsParseDone() {
    if (!ConnectionProfile::IsParseDone()) {
        return false;
    }
    return http_req_parser_.IsEnd();
}

http::ParserBase *ConnectionFrom::GetParser() { return &http_req_parser_; }

void ConnectionFrom::MakeRecvContext() {
    if (!IsParseDone()) {
        LogE("recv ctx cannot be made when the parsing NOT done")
        return;
    }
    recv_ctx_.fd = socket_.FD();
    recv_ctx_.is_post = http_req_parser_.IsMethodPost();
    if (recv_ctx_.is_post) {
        recv_ctx_.http_body.SetPtr(http_req_parser_.GetBody());
        recv_ctx_.http_body.SetLength(http_req_parser_.GetContentLength());
        recv_ctx_.http_body.ShallowCopy(true);
    }
    std::string &url = http_req_parser_.GetRequestUrl();
    recv_ctx_.full_url = std::string(url);
    recv_ctx_.send_context = &send_ctx_;
}

ConnectionFrom::~ConnectionFrom() = default;


}
