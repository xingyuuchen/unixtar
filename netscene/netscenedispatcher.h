#pragma once
#include "netscenebase.h"
#include "singleton.h"
#include "webserver.h"
#include <vector>
#include <map>
#include <cassert>
#include <mutex>
#include "log.h"


/**
 * Service Distribution
 */
class NetSceneDispatcher final {
    
    SINGLETON(NetSceneDispatcher, )
    
  public:
    ~NetSceneDispatcher();
    
    
    template<class NetSceneImpl/* : public NetSceneBase */, class ...Args>
    void
    RegisterNetScene(Args &&..._init_args) {
        NetSceneBase *net_scene = new NetSceneImpl(_init_args...);
        
        int type = net_scene->GetType();
        std::lock_guard<std::mutex> lock(selector_mutex_);
    
        assert(selectors_.size() == type);
        selectors_.push_back(net_scene);
    
        if (!net_scene->IsUseProtobuf() && net_scene->Route()) {
            // Check url route conflict.
            assert(route_map_.find(net_scene->Route()) == route_map_.end());
            
            LogI("Register route: %s", net_scene->Route())
            route_map_[net_scene->Route()] = net_scene->GetType();
        }
    }
    
    class NetSceneWorker : public WebServer::WorkerThread {
      public:
        ~NetSceneWorker() override;
        
        void HandleImpl(tcp::RecvContext::Ptr) override;
        
        void HandleOverload(tcp::RecvContext::Ptr) override;
    
        void HandleException(std::exception &ex) override;
    
        static void HandleWebSocket(const tcp::RecvContext::Ptr&);
        
        // debug only
        static void WriteFakeWsResp(const tcp::RecvContext::Ptr&);

      private:
        static void PackHttpRespPacket(NetSceneBase *_net_scene,
                                         AutoBuffer &_http_msg);
    };
    
  private:
    
    NetSceneBase *__MakeNetScene(int _type);
    
    int __GetNetSceneTypeByRoute(std::string &_full_url);
    
    static bool __DynamicRouteMatch(std::string &_dynamic, std::string &_route);
    
  private:
    std::vector<NetSceneBase *>     selectors_;
    std::mutex                      selector_mutex_;
    std::map<std::string, int>      route_map_;
    std::mutex                      map_mutex_;
    
};

