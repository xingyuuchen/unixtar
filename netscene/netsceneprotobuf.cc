#include "netsceneprotobuf.h"
#include "constantsprotocol.h"
#include "http/headerfield.h"


NetSceneProtobuf::NetSceneProtobuf()
        : NetSceneBase() {
    errcode_ = kOK;
    errmsg_ = "OK";
}

bool NetSceneProtobuf::IsUseProtobuf() {
    return true;
}

void *NetSceneProtobuf::Data() {
    return nullptr;
}

size_t NetSceneProtobuf::Length() {
    return 0;
}

const char *NetSceneProtobuf::Route() {
    return nullptr;
}

const char *NetSceneProtobuf::ContentType() {
    return http::HeaderField::kOctetStream;
}

int NetSceneProtobuf::DoScene(const std::string &_in_buffer) {
    int ret = DoSceneImpl(_in_buffer);
    
    RespMessage *resp = GetRespMessage();
    if (resp) {
        base_resp_.set_errcode(errcode_);
        base_resp_.set_errmsg(errmsg_);
        base_resp_.set_net_scene_resp_buff(resp->SerializeAsString());
    }
    base_resp_.SerializeToString(&resp_buffer_);
    
    return ret;
}

NetSceneProtobuf::~NetSceneProtobuf() = default;

