#include "netscenedispatcher.h"
#include <stdio.h>
#include "basenetscenereq.pb.h"
#include "netscene_getindexpage.h"
#include "netscene_hellosvr.h"
#include "log.h"


NetSceneDispatcher::NetSceneDispatcher() {
    std::unique_lock<std::mutex> lock(mutex_);
    selectors_.push_back(new NetSceneGetIndexPage());
    selectors_.push_back(new NetSceneHelloSvr());
    
}

void NetSceneDispatcher::RegisterNetScene(NetSceneBase *_net_scene) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (_net_scene) {
        selectors_.push_back(_net_scene);
    }
}


NetSceneBase *NetSceneDispatcher::__MakeNetScene(int _type) {
    std::unique_lock<std::mutex> lock(mutex_);
    auto iter = std::find_if(selectors_.begin(), selectors_.end(),
                        [=] (NetSceneBase *find) { return find->GetType() == _type; });
    if (iter == selectors_.end()) {
        LogE(__FILE__, "[__MakeNetScene] NO such NetScene:"
             " type=%d, give up processing this request.", _type)
        return NULL;
    }
    return (*iter)->NewInstance();
}


/**
 * Dispatches the request to corresponding net_scene,
 * waiting for the latter finished processing.
 *
 * @return
 */
NetSceneBase *NetSceneDispatcher::Dispatch(SOCKET _conn_fd, const AutoBuffer *_in_buffer) {
    int type;
    std::string req_buffer;
    do {
        if (_in_buffer == NULL || _in_buffer->Ptr() == NULL) {
            LogI(__FILE__, "[Dispatch] return index page.")
            type = 0;
            break;
        }
        LogI(__FILE__, "[Dispatch] _in_buffer.len: %zd", _in_buffer->Length());
    
        BaseNetSceneReq::BaseNetSceneReq base_req;
        base_req.ParseFromArray(_in_buffer->Ptr(), _in_buffer->Length());
    
        if (!base_req.has_net_scene_type()) {
            LogI(__FILE__, "[Dispatch] base_req.has_net_scene_type(): false")
            return NULL;
        }
        type = base_req.net_scene_type();
        if (!base_req.has_net_scene_req_buff()) {
            LogI(__FILE__, "[Dispatch] type(%d), base_req.has_net_scene_req_buff(): false", type)
            return NULL;
        }
        req_buffer = base_req.net_scene_req_buff();
    } while (false);
    
    LogI(__FILE__, "[Dispatch] dispatch to type %d", type)
    
    NetSceneBase *net_scene = __MakeNetScene(type);
    if (net_scene) {
        net_scene->SetSocket(_conn_fd);
        net_scene->DoScene(req_buffer);
    }
    return net_scene;
}


NetSceneDispatcher::~NetSceneDispatcher() {
    std::unique_lock<std::mutex> lock(mutex_);
    for (auto iter = selectors_.begin(); iter != selectors_.end(); iter++) {
        delete *iter, *iter = NULL;
    }
}
