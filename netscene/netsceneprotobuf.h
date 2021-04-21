#pragma once
#include "netscenebase.h"
#include "autogen/basenetsceneresp.pb.h"


class NetSceneProtobuf : public NetSceneBase {
  public:
    
    NetSceneProtobuf();
    
    ~NetSceneProtobuf() override;
    
    NetSceneBase *NewInstance() override = 0;
    
    /**
     * Do not override, if you do not want to use protobuf,
     * derived from {@class NetSceneCustom}
     */
    bool IsUseProtobuf() final;
    
    const char *ContentType() final;
    
    
    using RespMessage = google::protobuf::Message;
    /**
     * @return: Your response to front-end, which will be used by
     * framework to populate the http response.
     */
    virtual RespMessage *GetRespMessage() = 0;
    
    
    /**
     *
     * Protobuf use google::protobuf::Message to transmit message.
     * Do not override these three.
     *
     * If you do want to use theme,
     * derived from {@class NetSceneCustom}
     */
    void *Data() final;
    size_t Length() final;
    const char *Route() final;
    
    int DoScene(const std::string &_in_buffer) final;

  protected:
    int                                 errcode_;
    std::string                         errmsg_;
    
  private:
    using NetSceneBase::resp_buffer_;
    BaseNetSceneResp::BaseNetSceneResp  base_resp_;
    
};

