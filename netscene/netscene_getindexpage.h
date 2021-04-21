#pragma once
#include "netscenecustom.h"
#include <mutex>


class NetSceneGetIndexPage : public NetSceneCustom {
  public:
    
    NetSceneGetIndexPage();
    
    int GetType() override;
    
    NetSceneBase *NewInstance() override;
    
    int DoSceneImpl(const std::string &_in_buffer) override;
    
    void *Data() override;
    
    size_t Length() override;
    
    const char *Route() override;
    
    const char *ContentType() override;

private:
    char                        resp_[128] {0, };
    
    static const char *const    kUrlRoute;
    static const char *const    kRespFormat;
    static std::mutex           mutex_;
    
};
