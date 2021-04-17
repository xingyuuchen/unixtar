#include "netscenebase.h"
#include "log.h"
#include "headerfield.h"
#include "socketepoll.h"


NetSceneBase::NetSceneBase() = default;

NetSceneBase::~NetSceneBase() = default;

void NetSceneBase::__ShowHttpHeader(AutoBuffer &_out) {
    for (size_t i = 0; i < _out.Length(); ++i) {
        if (*_out.Ptr(i) == 0x0d || *_out.Ptr(i) == 0x0a) {
            LogI("0x%x ", *_out.Ptr(i))
        } else {
            LogI("0x%x %c", *_out.Ptr(i), *_out.Ptr(i))
        }
    }
}

std::string &NetSceneBase::GetRespBuffer() { return resp_buffer_; }

