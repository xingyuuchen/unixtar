#include "netscene_404notfound.h"
#include <cassert>
#include "constantsprotocol.h"
#include "fileutil.h"
#include "log.h"


std::string NetScene404NotFound::k404Resp;

NetScene404NotFound::NetScene404NotFound()
        : NetSceneCustom() {
    NETSCENE_INIT_START
        std::string curr(__FILE__);
        std::string path404 = curr.substr(0, curr.rfind('/')) + "/res/404.html";
        
        if (!file::ReadFile(path404.c_str(), k404Resp)) {
            LogE("read 404.html failed")
            assert(false);
        }
    NETSCENE_INIT_END
}

int NetScene404NotFound::GetType() {
    return kNetSceneType404NotFound;
}

NetSceneBase *NetScene404NotFound::NewInstance() {
    return new NetScene404NotFound();
}

int NetScene404NotFound::DoSceneImpl(const std::string &_in_buffer) {
    // nothing to do
    return 0;
}

void *NetScene404NotFound::Data() {
    return (void *) k404Resp.data();
}

size_t NetScene404NotFound::Length() {
    return k404Resp.size();
}

char *NetScene404NotFound::Route() {
    return nullptr;
}
