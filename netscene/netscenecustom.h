#pragma once
#include "netscenebase.h"
#include "http/headerfield.h"


class NetSceneCustom : public NetSceneBase {
  public:
    
    NetSceneCustom();
    
    ~NetSceneCustom() override;
    
    NetSceneBase *NewInstance() override = 0;
    
    bool IsUseProtobuf() final;
    
    int DoScene(const std::string &_in_buffer) final;
    
    /**
     * If you use your custom message transform way other than protobuf,
     * override these two functions, informing the framework the pointer of
     * your data and how long it is.
     */
    void *Data() override = 0;
    
    size_t Length() override = 0;
    
    /**
     * A NetScene can choose to bind a url route.
     * This will be used when the request-body does not specify the NetScene type.
     */
    const char *Route() override = 0;
    
    /**
     * Default is "text/html"
     */
    const char *ContentType() override;

  private:
    using NetSceneBase::resp_buffer_;
    
};

