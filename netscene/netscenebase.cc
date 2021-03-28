#include "netscenebase.h"
#include "log.h"
#include "headerfield.h"
#include "socketepoll.h"
#include "constantsprotocol.h"
#include <errno.h>


NetSceneBase::NetSceneBase(bool _use_protobuf)
        : use_protobuf_(_use_protobuf) {
    errcode_ = kOK;
    errmsg_ = "OK";
    
}


int NetSceneBase::DoScene(const std::string &_in_buffer) {
    int ret = DoSceneImpl(_in_buffer);

    RespMessage *resp = GetRespMessage();
    if (use_protobuf_ && resp) {
        base_resp_.set_errcode(errcode_);
        base_resp_.set_errmsg(errmsg_);
        base_resp_.set_net_scene_resp_buff(resp->SerializeAsString());
    }
    
    if (use_protobuf_) {
        base_resp_.SerializeToString(&resp_buffer_);
    } else {
        resp_buffer_ = std::string((const char *) Data(), Length());
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

std::string &NetSceneBase::GetRespBuffer() { return resp_buffer_; }

bool NetSceneBase::UseProtobuf() const { return use_protobuf_; }

