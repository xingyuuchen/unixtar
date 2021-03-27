#ifndef OI_SVR_NETSCENEDISPATCHER_H
#define OI_SVR_NETSCENEDISPATCHER_H
#include "netscenebase.h"
#include "singleton.h"
#include "server.h"
#include <vector>
#include <mutex>


/**
 * 业务分发枢纽
 */
class NetSceneDispatcher final {
    
    SINGLETON(NetSceneDispatcher, )
    
  public:
    ~NetSceneDispatcher();
    
    void RegisterNetScene(NetSceneBase* _net_scene);
    
    class NetSceneWorker : public Server::WorkerThread {
      public:
        ~NetSceneWorker() override;
        
        void HandleImpl(Tcp::RecvContext *_recv_ctx) override;
        
        void HandleException(std::exception &ex) override;

    private:
        static void __PackHttpResp(NetSceneBase *_net_scene,
                                   AutoBuffer &_http_msg);
    };
    
  private:
    
    NetSceneBase *__MakeNetScene(int _type);
    
  private:
    std::vector<NetSceneBase *>     selectors_;
    std::mutex                      mutex_;
    
};


#endif //OI_SVR_NETSCENEDISPATCHER_H
