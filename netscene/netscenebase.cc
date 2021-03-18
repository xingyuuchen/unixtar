#include "netscenebase.h"
#include "../http/httpresponse.h"
#include "../http/firstline.h"
#include "log.h"
#include "headerfield.h"
#include "socketepoll.h"
#include <errno.h>


NetSceneBase::NetSceneBase()
        : status_code_(200)
        , status_desc_("OK")
        , socket_(-1) {
    http_headers_[http::HeaderField::KContentType] = http::HeaderField::KOctetStream;
    http_headers_[http::HeaderField::KConnection] = http::HeaderField::KConnectionClose;
    
}

void NetSceneBase::SetSocket(SOCKET _socket) {
    if (_socket > 0) { socket_ = _socket; }
}


int NetSceneBase::DoScene(const std::string &_in_buffer) {
    int ret = DoSceneImpl(_in_buffer);
    PackHttpMsg();
    return ret;
}

void NetSceneBase::CopyRespToSendBody(std::string &_resp, size_t _size) {
    resp_body_.Reset();
    LogI(__FILE__, "[CopyRespToSendBody] type%d: resp body len = %zd", GetType(), _size);
    resp_body_.Write(_resp.data(), _size);
}

int NetSceneBase::PackHttpMsg() {
    resp_msg_.Reset();
    http::response::Pack(http::kHTTP_1_1, status_code_,
                         status_desc_, http_headers_, resp_msg_, resp_body_);
    LogI(__FILE__, "[PackHttpMsg] resp_msg_ len: %ld", resp_msg_.Length())
//    __ShowHttpHeader(out_buff);
    resp_body_.Reset();
    return 0;
}

void NetSceneBase::__ShowHttpHeader(AutoBuffer &_out) {
    for (size_t i = 0; i < _out.Length() - resp_body_.Length(); ++i) {
        if (*_out.Ptr(i) == 0x0d || *_out.Ptr(i) == 0x0a) {
            LogI(__FILE__, "0x%x ", *_out.Ptr(i))
        } else {
            LogI(__FILE__, "0x%x %c", *_out.Ptr(i), *_out.Ptr(i))
        }
    }
}

AutoBuffer *NetSceneBase::GetHttpResp() { return &resp_msg_; }

int NetSceneBase::GetSocket() const { return socket_; }
