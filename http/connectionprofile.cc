#include "connectionprofile.h"
#include <cstring>
#include "timeutil.h"
#include "log.h"
#include "socket/unixsocket.h"


namespace tcp {

const uint64_t ConnectionProfile::kDefaultTimeout = 60 * 1000;

ConnectionProfile::ConnectionProfile(uint32_t _uid, int _fd,
                                     std::string _src_ip, uint16_t _src_port)
        : type_(kFrom)
        , uid_(_uid)
        , src_ip_(std::move(_src_ip))
        , src_port_(_src_port)
        , dst_port_(0)
        , socket_(_fd)
        , application_protocol_(TApplicationProtocol::kHttp1_1)
        , record_(::gettickcount())
        , timeout_millis_(kDefaultTimeout)
        , timeout_ts_(record_ + timeout_millis_) {
    
    if (_fd < 0) {
        socket_.Set(INVALID_SOCKET);
    }
    
}


ConnectionProfile::ConnectionProfile(uint32_t _uid,
                                     std::string _dst_ip,
                                     uint16_t _dst_port)
        : type_(kTo)
        , uid_(_uid)
        , dst_ip_(std::move(_dst_ip))
        , src_port_(0)
        , dst_port_(_dst_port)
        , socket_(INVALID_SOCKET)
        , application_protocol_(TApplicationProtocol::kHttp1_1)
        , record_(::gettickcount())
        , timeout_millis_(kDefaultTimeout)
        , timeout_ts_(record_ + timeout_millis_) {
    
    if (socket_.Create(AF_INET, SOCK_STREAM)) {
        LogE("Create socket_ failed")
    }
}

int ConnectionProfile::Connect() {
    return socket_.Connect(dst_ip_, dst_port_);
}

int ConnectionProfile::Receive() {
    AutoBuffer *recv_buff = http_parser_.GetBuff();
    
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
            MakeContext();
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
    
    http_parser_.DoParse();
    
    if (http_parser_.IsErr()) {
        LogI("parser error")
        return -2;
    }
    
    return 0;
}


bool ConnectionProfile::IsParseDone() {
    if (application_protocol_ != TApplicationProtocol::kHttp1_1) {
        return false;
    }
    return http_parser_.IsEnd();
}

void ConnectionProfile::CloseTcpConnection() {
    socket_.Close();
}

ConnectionProfile::TApplicationProtocol ConnectionProfile::GetApplicationProtocol() const {
    return application_protocol_;
}


http::request::Parser *ConnectionProfile::GetHttpParser() { return &http_parser_; }

SOCKET ConnectionProfile::FD() const { return socket_.FD(); }

http::RecvContext *ConnectionProfile::GetRecvContext() { return &recv_ctx_; }

SendContext *ConnectionProfile::GetSendContext() { return &send_ctx_; }

uint64_t ConnectionProfile::GetTimeoutTs() const { return timeout_ts_; }

std::string &ConnectionProfile::SrcIp() { return src_ip_; }

std::string &ConnectionProfile::DstIp() { return dst_ip_; }

uint16_t ConnectionProfile::SrcPort() const { return src_port_; }

uint16_t ConnectionProfile::DstPort() const { return dst_port_; }

bool ConnectionProfile::IsTimeout(uint64_t _now) const {
    if (_now == 0) {
        _now = ::gettickcount();
    }
    return _now > timeout_ts_;
}

ConnectionProfile::TType ConnectionProfile::GetType() const { return type_; }

bool ConnectionProfile::IsTypeValid() const {
    TType type = GetType();
    return type == kTo || type == kFrom;
}

void ConnectionProfile::MakeContext() {
    SOCKET fd = socket_.FD();
    if (fd < 0) {
        return;
    }
    if (!IsParseDone()) {
        LogE("recv ctx cannot be made when the parsing hasn't done")
        return;
    }
    recv_ctx_.fd = fd;
    
    recv_ctx_.is_post = http_parser_.IsMethodPost();
    if (recv_ctx_.is_post) {
        recv_ctx_.http_body.SetPtr(http_parser_.GetBody());
        recv_ctx_.http_body.SetLength(http_parser_.GetContentLength());
        recv_ctx_.http_body.ShallowCopy(true);
    }
    std::string &url = http_parser_.GetRequestUrl();
    recv_ctx_.full_url = std::string(url);
    recv_ctx_.send_context = &send_ctx_;
    MakeSendContext();
}

void ConnectionProfile::MakeSendContext() {
    send_ctx_.connection_uid = Uid();
    send_ctx_.fd = socket_.FD();
}

ConnectionProfile::~ConnectionProfile() = default;

}
