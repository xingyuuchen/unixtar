#include "netscenedispatcher.h"
#include <stdio.h>
#include "basenetscenereq.pb.h"
#include "netscene_getindexpage.h"
#include "netscene_hellosvr.h"
#include "netscene_404notfound.h"
#include "timeutil.h"
#include "http/httpresponse.h"
#include "constantsprotocol.h"


NetSceneDispatcher::NetSceneDispatcher() {
    RegisterNetScene<NetSceneGetIndexPage>();
    RegisterNetScene<NetSceneHelloSvr>();
    RegisterNetScene<NetScene404NotFound>();
    
    // pad nullptr to unused reserved NetScenes.
    for (size_t i = selectors_.size(); i <= kReservedTypeOffset; ++i) {
        selectors_.push_back(nullptr);
    }
    
    WebServer::Instance().SetWorker<NetSceneWorker>();
}


NetSceneBase *NetSceneDispatcher::__MakeNetScene(int _type) {
    std::lock_guard<std::mutex> lock(selector_mutex_);
    if (selectors_.size() <= _type) {
        LogE("No such NetScene: type=%d, "
             "give up processing this request.", _type)
        return selectors_[0]->NewInstance();
    }
    NetSceneBase *select = selectors_[_type];
    assert(select->GetType() == _type);
    return select->NewInstance();
}

bool NetSceneDispatcher::__DynamicRouteMatch(std::string &_dynamic, std::string &_route) {
    // TODO: now only support format like: /xxx/xxx/*
    if (_dynamic.empty()) { return false; }
    if (_dynamic.at(_dynamic.size() - 1) != '*') {
        return false;
    }
    if (_dynamic.size() > _route.size()) {
        return false;
    }
    for (int i = 0; i < _dynamic.size() - 1; ++i) {
        if (_dynamic.at(i) != _route.at(i)) {
            return false;
        }
    }
    return true;
}

int NetSceneDispatcher::__GetNetSceneTypeByRoute(std::string &_full_url) {
    std::lock_guard<std::mutex> lock(map_mutex_);
    std::string route;
    
    std::string::size_type route_end = _full_url.find('?');
    if (route_end == std::string::npos) {
        route = _full_url;
    } else {
        route = _full_url.substr(0, route_end);
    }
    if (route_map_.find(route) != route_map_.end()) {
        return route_map_[route];
    }
    for (auto &rou : route_map_) {
        std::string dynamic = rou.first;
        if (__DynamicRouteMatch(dynamic, route)) {
            return rou.second;
        }
    }
    LogI("dynamic route NOT matched: %s", route.c_str())
    
    return kNetSceneType404NotFound;
}

NetSceneDispatcher::NetSceneWorker::~NetSceneWorker() = default;

void NetSceneDispatcher::NetSceneWorker::HandleImpl(http::RecvContext *_recv_ctx) {
    if (!_recv_ctx) {
        return;
    }
    SOCKET fd = _recv_ctx->fd;
    AutoBuffer &http_body = _recv_ctx->http_body;
    
    int type = kNetSceneType404NotFound;
    std::string req_buffer;
    do {
        if (!_recv_ctx->is_post) {
            std::string &full_url = _recv_ctx->full_url;
            LogI("fd(%d) GET, full_url: %s", fd, full_url.c_str())
            type = NetSceneDispatcher::Instance().__GetNetSceneTypeByRoute(full_url);
            break;
        }
        if (!http_body.Ptr() || http_body.Length() <= 0) {
            LogI("POST but no http body, return 404")
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
        net_scene->DoScene(_recv_ctx->full_url);
    }
    uint64_t cost = ::gettickcount() - start;
    LogI("fd(%d) type(%d), cost %llu ms", fd, type, cost)
    
    AutoBuffer &http_resp_msg = _recv_ctx->send_context->buffer;
    __PackHttpResp(net_scene, http_resp_msg);
    
    delete net_scene, net_scene = nullptr;

}

void NetSceneDispatcher::NetSceneWorker::HandleOverload(http::RecvContext *_recv_ctx) {
    if (!_recv_ctx) {
        return;
    }
    std::string resp_str = "server is busy now";
    std::string status_desc = "OK";
    int resp_code = 200;
    
    std::string resp;
    std::map<std::string, std::string> headers;
    if (!_recv_ctx->is_post) {
        resp = resp_str;
        headers[http::HeaderField::kContentType] = http::HeaderField::kTextPlain;
        
    } else {
        headers[http::HeaderField::kContentType] = http::HeaderField::kOctetStream;
        BaseNetSceneResp::BaseNetSceneResp base_resp;
        base_resp.set_errcode(kSvrOverload);
        base_resp.set_errmsg(resp);
        base_resp.SerializeToString(&resp);
    }
    http::response::Pack(http::kHTTP_1_1, resp_code,status_desc,
                         headers, _recv_ctx->send_context->buffer, resp);
}

void NetSceneDispatcher::NetSceneWorker::HandleException(std::exception &ex) {
    LogE("%s", ex.what())
    running_ = false;
}


void NetSceneDispatcher::NetSceneWorker::__PackHttpResp(
        NetSceneBase *_net_scene, AutoBuffer &_http_msg) {
    
    std::map<std::string, std::string> headers;
    
    if (_net_scene->IsUseProtobuf()) {
        headers[http::HeaderField::kContentType] = http::HeaderField::kOctetStream;
    } else {
        headers[http::HeaderField::kContentType] = _net_scene->ContentType();
    }
    headers[http::HeaderField::kConnection] = http::HeaderField::kConnectionClose;
    
    std::string status_desc = "OK";
    int resp_code = 200;
    
    http::response::Pack(http::kHTTP_1_1, resp_code,status_desc,
                         headers, _http_msg, _net_scene->GetRespBuffer());
    
}

NetSceneDispatcher::~NetSceneDispatcher() {
    std::lock_guard<std::mutex> lock(selector_mutex_);
    for (auto &selector : selectors_) {
        if (selector) {
            delete selector, selector = nullptr;
        }
    }
}
