#ifndef OI_SVR_NETSCENEDISPATCHER_H
#define OI_SVR_NETSCENEDISPATCHER_H
#include "netscenebase.h"
#include "singleton.h"
#include <vector>
#include <mutex>


/**
 * 业务分发枢纽
 */
class NetSceneDispatcher final {
    
    SINGLETON(NetSceneDispatcher, )
    
  public:
    ~NetSceneDispatcher();
    
    NetSceneBase *Dispatch(SOCKET _conn_fd, const AutoBuffer *_buffer);
    
    void RegisterNetScene(NetSceneBase* _net_scene);
    
  private:
    
    NetSceneBase *__MakeNetScene(int _type);
    
  private:
    std::vector<NetSceneBase *>     selectors_;
    std::mutex                      mutex_;
    
};


#endif //OI_SVR_NETSCENEDISPATCHER_H
