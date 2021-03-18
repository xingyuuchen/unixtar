#ifndef OI_SVR_NETSCENE_HELLOSVR_H
#define OI_SVR_NETSCENE_HELLOSVR_H

#include "netscenebase.h"


/**
 * this NetScene uses plain-text instead of protobuf.
 */
class NetSceneHelloSvr : public NetSceneBase {
  public:
    NetSceneHelloSvr();
    
    int GetType() override;
    
    NetSceneBase *NewInstance() override;
    
    int DoSceneImpl(const std::string &_in_buffer) override;
    
  private:

};


#endif //OI_SVR_NETSCENE_HELLOSVR_H
