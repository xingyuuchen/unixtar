#pragma once
#include "netscenecustom.h"
#include <string>


class NetScene404NotFound : public NetSceneCustom {
  public:
    
    NetScene404NotFound();
    
    int GetType() override;
    
    NetSceneBase *NewInstance() override;
    
    int DoSceneImpl(const std::string &_in_buffer) override;
    
    void *Data() override;
    
    size_t Length() override;
    
    char *Route() override;
    
  private:
    static std::string  k404Resp;
    
};

