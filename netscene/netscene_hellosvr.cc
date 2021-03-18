#include "netscene_hellosvr.h"
#include "constantsprotocol.h"
#include "headerfield.h"


NetSceneHelloSvr::NetSceneHelloSvr() : NetSceneBase() {
    http_headers_[http::HeaderField::KContentType] = http::HeaderField::KPlainText;
}

int NetSceneHelloSvr::GetType() { return kNetSceneTypeHelloSvr; }

NetSceneBase *NetSceneHelloSvr::NewInstance() { return new NetSceneHelloSvr(); }

int NetSceneHelloSvr::DoSceneImpl(const std::string &_in_buffer) {
    static int visit_times_since_last_boot_ = 0;
    std::string resp = "If you see this, the server is running normally, "
                   + std::to_string(++visit_times_since_last_boot_) + " visits since last boot.";
    CopyRespToSendBody(resp, resp.size());
    return 0;
}
