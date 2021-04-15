#include "netscenedispatcher.h"
#include <stdio.h>
#include "basenetscenereq.pb.h"
#include "netscene_getindexpage.h"
#include "netscene_hellosvr.h"
#include "log.h"
#include "timeutil.h"
#include "http/httpresponse.h"


NetSceneDispatcher::NetSceneDispatcher() {
    std::lock_guard<std::mutex> lock(selector_mutex_);
    selectors_.push_back(new NetSceneGetIndexPage());
    selectors_.push_back(new NetSceneHelloSvr());
    
}

void NetSceneDispatcher::RegisterNetScene(NetSceneBase *_net_scene) {
    if (!_net_scene) {
        LogE("!_net_scene")
        return;
    }
    int type = _net_scene->GetType();
    std::lock_guard<std::mutex> lock(selector_mutex_);
    assert(selectors_.size() == type);
    selectors_.push_back(_net_scene);
    if (_net_scene->Route()) {
        if (route_map_.find(_net_scene->Route()) != route_map_.end()) {
            LogE("url route \"%s\" CONFLICT, check NetScene type %d",
                 _net_scene->Route(), route_map_[_net_scene->Route()])
        }
        route_map_[_net_scene->Route()] = _net_scene->GetType();
    }
}


NetSceneBase *NetSceneDispatcher::__MakeNetScene(int _type) {
    std::lock_guard<std::mutex> lock(selector_mutex_);
    if (selectors_.size() <= _type) {
        LogE("No such NetScene: type=%d, "
             "give up processing this request.", _type)
        return nullptr;
    }
    NetSceneBase *select = selectors_[_type];
    assert(select->GetType() == _type);
    return select->NewInstance();
}

int NetSceneDispatcher::__GetNetSceneTypeByRoute(std::string &_route) {
    std::lock_guard<std::mutex> lock(map_mutex_);
    if (route_map_.find(_route) == route_map_.end()) {
        return 0;
    }
    return route_map_[_route];
}

NetSceneDispatcher::NetSceneWorker::~NetSceneWorker() = default;

void NetSceneDispatcher::NetSceneWorker::HandleImpl(http::RecvContext *_recv_ctx) {
    if (!_recv_ctx) {
        return;
    }
    if (!net_thread_) {
        LogE("wtf? No net_thread!")
        return;
    }
    SOCKET fd = _recv_ctx->fd;
    AutoBuffer &http_body = _recv_ctx->http_body;
    
    int type;
    std::string req_buffer;
    do {
        if (!_recv_ctx->is_post) {
            std::string &url_route = _recv_ctx->url_route;
            LogI("fd(%d) GET, route: %s", fd, url_route.c_str())
            type = NetSceneDispatcher::Instance().__GetNetSceneTypeByRoute(url_route);
            break;
        }
        if (!http_body.Ptr() || http_body.Length() <= 0) {
            LogI("POST but no http body, return index page.")
            type = 0;
            break;
        }
        LogI("fd(%d) http_body.len: %zd", fd, http_body.Length());
        BaseNetSceneReq::BaseNetSceneReq base_req;
        base_req.ParseFromArray(http_body.Ptr(), http_body.Length());
    
        if (!base_req.has_net_scene_type()) {
            LogI("fd(%d) base_req.has_net_scene_type(): false", fd)
            return;
        }
        type = base_req.net_scene_type();
        if (!base_req.has_net_scene_req_buff()) {
            LogI("fd(%d), type(%d), base_req.has_net_scene_req_buff(): false", fd, type)
            return;
        }
        req_buffer = base_req.net_scene_req_buff();
    } while (false);
    
    LogI("fd(%d) dispatch to type %d", fd, type)
    
    NetSceneBase *net_scene = NetSceneDispatcher::Instance().__MakeNetScene(type);
    if (!net_scene) {
        LogE("fd(%d) !net_scene", fd)
        return;
    }
    
    uint64_t start = ::gettickcount();
    if (_recv_ctx->is_post) {
        net_scene->DoScene(req_buffer);
    } else {
        net_scene->DoScene(_recv_ctx->url_route);
    }
    uint64_t cost = ::gettickcount() - start;
    LogI("fd(%d) type(%d), cost %llu ms", fd, type, cost)
    
    AutoBuffer &http_resp_msg = _recv_ctx->send_context->buffer;
    __PackHttpResp(net_scene, http_resp_msg);
    
    delete net_scene, net_scene = nullptr;
    
    Server::SendQueue *send_queue = net_thread_->GetSendQueue();
    send_queue->push_back(_recv_ctx->send_context);
    
    net_thread_->NotifySend();
    
}

void NetSceneDispatcher::NetSceneWorker::HandleException(std::exception &ex) {
    LogE("%s", ex.what())
    running_ = false;
}


void NetSceneDispatcher::NetSceneWorker::__PackHttpResp(
        NetSceneBase *_net_scene, AutoBuffer &_http_msg) {
    
    std::map<std::string, std::string> headers;
    
    if (_net_scene->IsUseProtobuf()) {
        headers[http::HeaderField::KContentType] = http::HeaderField::KOctetStream;
    } else {
        headers[http::HeaderField::KContentType] = http::HeaderField::KPlainText;
    }
    headers[http::HeaderField::KConnection] = http::HeaderField::KConnectionClose;
    
    std::string status_desc = "OK";
    int resp_code = 200;
    
    http::response::Pack(http::kHTTP_1_1, resp_code,status_desc,
                         headers, _http_msg, _net_scene->GetRespBuffer());
    
}

NetSceneDispatcher::~NetSceneDispatcher() {
    std::lock_guard<std::mutex> lock(selector_mutex_);
    for (auto &selector : selectors_) {
        delete selector, selector = nullptr;
    }
}
