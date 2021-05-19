#pragma once

#include <cstring>
#include <cstdint>


namespace md {

class Sha1 {
  public:
    
    static void Digest(const unsigned char *_msg,
                       size_t _msg_len, unsigned char *_res);

  private:
    
    struct Sha1Context {
        uint32_t states[5];
    };
    
    static void Sha1ContextInit(Sha1Context &_ctx);
    
    /**
     * Hash a single 512bit(64 bytes) block.
     */
    static void Sha1Transform(uint32_t _state[5],
                              const unsigned char _buf[64]);
    
    
    static void UnitTest();
};

}

