#ifndef OI_SVR_NETSCENEDISPATCHER_H
#define OI_SVR_NETSCENEDISPATCHER_H
#include "netscenebase.h"
#include "singleton.h"
#include "server.h"
#include <vector>
#include <map>
#include <cassert>
#include <mutex>
#include "log.h"


/**
 * 业务分发枢纽
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
    
    class NetSceneWorker : public Server::WorkerThread {
      public:
        ~NetSceneWorker() override;
        
        void HandleImpl(http::RecvContext *_recv_ctx) override;
        
        void HandleOverload(http::RecvContext *_recv_ctx) override;
    
        void HandleException(std::exception &ex) override;

      private:
        static void __PackHttpResp(NetSceneBase *_net_scene,
                                   AutoBuffer &_http_msg);
    };
    
  private:
    
    NetSceneBase *__MakeNetScene(int _type);
    
    int __GetNetSceneTypeByRoute(std::string &_full_url);
    
    bool __DynamicRouteMatch(std::string &_dynamic, std::string &_route);
    
  private:
    std::vector<NetSceneBase *>     selectors_;
    std::mutex                      selector_mutex_;
    std::map<std::string, int>      route_map_;
    std::mutex                      map_mutex_;
    
};


#endif //OI_SVR_NETSCENEDISPATCHER_H
