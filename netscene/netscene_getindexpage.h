#pragma once
#include "netscenebase.h"
#include <mutex>


class NetSceneGetIndexPage : public NetSceneBase {
  public:
    NetSceneGetIndexPage();
    
    int GetType() override;
    
    NetSceneBase *NewInstance() override;
    
    int DoSceneImpl(const std::string &_in_buffer) override;
    
    void *Data() override;
    
    size_t Length() override;
    
    bool IsUseProtobuf() override;
    
    char *Route() override;

private:
    char                        resp_[128] {0, };
    
    static const char *const    kUrlRoute;
    static const char *const    kRespFormat;
    static std::mutex           mutex_;
    
};
