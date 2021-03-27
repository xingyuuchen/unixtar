#include "netscenebase.h"
#include "log.h"
#include "headerfield.h"
#include "socketepoll.h"
#include "constantsprotocol.h"
#include <errno.h>


NetSceneBase::NetSceneBase(bool _use_protobuf)
        : status_code_(200)
        , status_desc_("OK")
        , use_protobuf_(_use_protobuf)
        , socket_(-1) {
    errcode_ = kOK;
    errmsg_ = "OK";
    
}


int NetSceneBase::DoScene(const std::string &_in_buffer) {
    if (socket_ < 0) {
        LogE(__FILE__, "[DoScene] did NOT set socket!")
        return -1;
    }
    int ret = DoSceneImpl(_in_buffer);

    RespMessage *resp = GetRespMessage();
    if (use_protobuf_ && resp) {
        base_resp_.set_errcode(errcode_);
        base_resp_.set_errmsg(errmsg_);
        base_resp_.set_net_scene_resp_buff(resp->SerializeAsString());
    }
    
    if (use_protobuf_) {
        base_resp_.SerializeToString(&http_body_);
    } else {
        http_body_ = std::string((const char *) Data(), Length());
    }
//    __ShowHttpHeader(out_buff);
    return ret;
}


void NetSceneBase::__ShowHttpHeader(AutoBuffer &_out) {
    for (size_t i = 0; i < _out.Length(); ++i) {
        if (*_out.Ptr(i) == 0x0d || *_out.Ptr(i) == 0x0a) {
            LogI(__FILE__, "0x%x ", *_out.Ptr(i))
        } else {
            LogI(__FILE__, "0x%x %c", *_out.Ptr(i), *_out.Ptr(i))
        }
    }
}

std::string &NetSceneBase::GetHttpBody() { return http_body_; }

int NetSceneBase::GetSocket() const { return socket_; }

void NetSceneBase::SetSocket(SOCKET _socket) {
    if (_socket > 0) {
        socket_ = _socket;
    }
}

bool NetSceneBase::UseProtobuf() const { return use_protobuf_; }

