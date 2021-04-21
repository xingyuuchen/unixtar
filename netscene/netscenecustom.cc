#include "netscenecustom.h"
#include "http/headerfield.h"

NetSceneCustom::NetSceneCustom() = default;

NetSceneCustom::~NetSceneCustom() = default;

bool NetSceneCustom::IsUseProtobuf() {
    return false;
}

int NetSceneCustom::DoScene(const std::string &_in_buffer) {
    int ret = DoSceneImpl(_in_buffer);
    resp_buffer_ = std::string((const char *) Data(), Length());
//    __ShowHttpHeader(out_buff);
    return ret;
}

const char *NetSceneCustom::ContentType() {
    return http::HeaderField::kTextHtml;
}
