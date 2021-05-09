#pragma once

#include "netsceneprotobuf.h"
#include "netscenehellosvr.pb.h"


/**
 * This is an NetSceneProtobuf example.
 *
 */
class NetSceneHelloSvr : public NetSceneProtobuf {
  public:
    
    NetSceneHelloSvr();
    
    /**
     * 1 is a reserved NetSceneType, which you can not use in your application.
     * The NetSceneType is the unique key to distinguish different NetScenes
     */
    int GetType() override;
    
    /**
     * @return: Your response to front-end, the framework will use it
     *          to populate the http response.
     */
    RespMessage *GetRespMessage() override;
    
    /**
     * @return: A new instance of such NetScene.
     */
    NetSceneBase *NewInstance() override;
    
    /**
     * Just get your business done here.
     */
    int DoSceneImpl(const std::string &_in_buffer) override;
    
  private:
    NetSceneHelloSvrProto::NetSceneHelloSvrResp resp_;

};

