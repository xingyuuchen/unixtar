#include "netscenedispatcher.h"
#include <cstdio>
#include "basenetscenereq.pb.h"
#include "netscene_getindexpage.h"
#include "netscene_hellosvr.h"
#include "netscene_404notfound.h"
#include "timeutil.h"
#include "http/httprequest.h"
#include "http/httpresponse.h"
#include "websocketpacket.h"
#include "constantsprotocol.h"


NetSceneDispatcher::NetSceneDispatcher() {
    RegisterNetScene<NetSceneGetIndexPage>();
    RegisterNetScene<NetSceneHelloSvr>();
    RegisterNetScene<NetScene404NotFound>();
    RegisterNetScene<NetSceneGetFavIcon>();
    
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

void NetSceneDispatcher::NetSceneWorker::HandleImpl(tcp::RecvContext::Ptr _recv_ctx) {
    TApplicationProtocol app_proto = _recv_ctx->application_packet->Protocol();
    if (app_proto != kWebSocket && app_proto != kHttp1_1) {
        LogE("unknown application protocol: %d", app_proto)
        return;
    }
    if (app_proto == kWebSocket) {
        LogI("dispatch to WebSocket")
        HandleWebSocket(_recv_ctx);
        return;
    }
    
    /* Handle Http */
    auto http_request = std::dynamic_pointer_cast<http::request::HttpRequest>(
            _recv_ctx->application_packet);
    
    SOCKET fd = _recv_ctx->fd;
    AutoBuffer *http_body = http_request->Body();
    
    int type = kNetSceneType404NotFound;
    std::string req_buffer;
    do {
        std::string &full_url = http_request->Url();
        type = NetSceneDispatcher::Instance().__GetNetSceneTypeByRoute(full_url);
        if (!http_request->IsMethodPost()) {
            LogI("fd(%d), GET, url: %s", fd, full_url.c_str())
            break;
        }
        if (type != kNetSceneType404NotFound) {
            LogI("fd(%d), POST, url: %s", fd, full_url.c_str())
            req_buffer = std::string(http_body->Ptr(), http_body->Length());
            break;
        }
        if (!http_body->Ptr() || http_body->Length() <= 0) {
            LogI("fd(%d), POST but no http body, return 404", fd)
            break;
        }
        LogI("fd(%d) http_body.len: %zd", fd, http_body->Length());
        BaseNetSceneReq::BaseNetSceneReq base_req;
        base_req.ParseFromArray(http_body->Ptr(), (int) http_body->Length());
    
        if (!base_req.has_net_scene_type()) {
            LogI("fd(%d) base_req.has_net_scene_type(): false", fd)
            break;
        }
        type = base_req.net_scene_type();
        if (!base_req.has_net_scene_req_buff()) {
            LogI("fd(%d), type(%d), base_req.has_net_scene_req_buff(): false", fd, type)
            break;
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
    if (http_request->IsMethodPost()) {
        net_scene->DoScene(req_buffer);
    } else {
        net_scene->DoScene(http_request->Url());
    }
    uint64_t cost = ::gettickcount() - start;
    LogI("fd(%d) type(%d), cost %llu ms", fd, type, cost)
    
    AutoBuffer &http_resp_msg = _recv_ctx->packet_back->buffer;
    PackHttpRespPacket(net_scene, http_resp_msg);
    
    delete net_scene, net_scene = nullptr;

}

void NetSceneDispatcher::NetSceneWorker::HandleOverload(tcp::RecvContext::Ptr _recv_ctx) {
    TApplicationProtocol app_proto = _recv_ctx->application_packet->Protocol();
    if (app_proto != kWebSocket && app_proto != kHttp1_1) {
        LogE("unknown application protocol: %d", app_proto)
        return;
    }
    
    std::string resp_str = "server is busy now";
    std::string resp;
    
    BaseNetSceneResp::BaseNetSceneResp base_resp;
    base_resp.set_errcode(kSvrOverload);
    base_resp.set_errmsg(resp_str);
    base_resp.SerializeToString(&resp);
    
    if (app_proto == kHttp1_1) {
        auto http_request = std::dynamic_pointer_cast<http::request::HttpRequest>(
                _recv_ctx->application_packet);
        int resp_code = 200;
        
        std::map<std::string, std::string> headers;
    
        if (http_request->IsMethodPost()) {
            headers[http::HeaderField::kContentType] = http::HeaderField::kOctetStream;
        } else {
            headers[http::HeaderField::kContentType] = http::HeaderField::kTextPlain;
            resp = resp_str;
        }
        http::response::Pack(http::kHTTP_1_1, resp_code,
                             http::StatusLine::kStatusDescOk, &headers,
                             _recv_ctx->packet_back->buffer, &resp);
        
    } else {
        ws::Pack(resp, _recv_ctx->packet_back->buffer);
    }
}

void NetSceneDispatcher::NetSceneWorker::HandleWebSocket(const tcp::RecvContext::Ptr& _recv_ctx) {
    assert(_recv_ctx->application_packet->Protocol() == kWebSocket);
    auto ws_packet = std::dynamic_pointer_cast<ws::WebSocketPacket>(
                _recv_ctx->application_packet);
    
    if (!ws_packet->IsHandShaken()) {
        bool success = ws_packet->DoHandShake();
    
        tcp::SendContext::Ptr packet_back = _recv_ctx->packet_back;
        auto &buffer = packet_back->buffer;
    
        http::HeaderField &resp_headers = ws_packet->ResponseHeaders();
        int resp_code = 101;
    
        if (!success) {
            resp_code = 404;
            LogE("WebSocket Hand Shake failed")
        }
        http::response::Pack(http::kHTTP_1_1, resp_code,
                             http::StatusLine::kStatusDescSwitchProtocol,
                             &resp_headers.AsMap(), packet_back->buffer);
        return;
    }
    LogI("payload: %s", ws_packet->Payload().c_str())
    
    WriteFakeWsResp(_recv_ctx);
}

void NetSceneDispatcher::NetSceneWorker::HandleException(std::exception &ex) {
    LogE("%s", ex.what())
    running_ = false;
}

void NetSceneDispatcher::NetSceneWorker::WriteFakeWsResp(tcp::RecvContext::Ptr _recv_ctx) {
    static uint64_t visit = 0;
    char resp[256];
    snprintf(resp, sizeof(resp), R"({"msg":"This is %llu visits to unixtar"})", ++visit);
    std::string resp_str(resp);
    // websocket can make send_context here
    // _recv_ctx->packet_push_others.push_back(new tcp::SendContext);
    ws::Pack(resp_str, _recv_ctx->packet_back->buffer);
}

void NetSceneDispatcher::NetSceneWorker::PackHttpRespPacket(
        NetSceneBase *_net_scene, AutoBuffer &_http_msg) {
    
    std::map<std::string, std::string> headers;
    
    _net_scene->CustomHttpHeaders(headers);
    
    if (_net_scene->IsUseProtobuf()) {
        headers[http::HeaderField::kContentType] = http::HeaderField::kOctetStream;
    } else {
        headers[http::HeaderField::kContentType] = _net_scene->ContentType();
    }
    headers[http::HeaderField::kConnection] = http::HeaderField::kConnectionClose;
    
    int resp_code = 200;
    
    http::response::Pack(http::kHTTP_1_1, resp_code, http::StatusLine::kStatusDescOk,
                         &headers, _http_msg, &_net_scene->GetRespBuffer());
    
}

NetSceneDispatcher::~NetSceneDispatcher() {
    std::lock_guard<std::mutex> lock(selector_mutex_);
    for (auto &selector : selectors_) {
        if (selector) {
            delete selector, selector = nullptr;
        }
    }
}
