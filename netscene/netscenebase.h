#ifndef OI_SVR_NETSCENEBASE_H
#define OI_SVR_NETSCENEBASE_H
#include <string>
#include <string.h>
#include <map>
#include "socket/unix_socket.h"
#include "autobuffer.h"
#include "autogen/basenetsceneresp.pb.h"

/**
 * Base class for all NetScenes.
 *
 * Responsible for:
 *      1. Specifying the net scene type;
 *      2. Packing http body into http message;
 * Note:
 *      1. NOT responsible for any network operation(recv, send, etc.);
 *      2. Business logic is Implemented the by subclasses by overriding DoSceneImpl.
 */
class NetSceneBase {

  public:
    NetSceneBase();
    
    /**
     *
     * @return The Unique NetScene Type, must be declared outside the framework.
     */
    virtual int GetType() = 0;
    
    /**
     *
     * @return: a new instance of a net scene.
     */
    virtual NetSceneBase *NewInstance() = 0;
    
    /**
     *
     * @param _in_buffer: the buffer used to parse to your custom data structure.
     * @return: the business logic implemented as you wish.
     */
    virtual int DoSceneImpl(const std::string &_in_buffer) = 0;
    
    virtual ~NetSceneBase() {}
    
    void SetSocket(SOCKET _socket);
    
    int DoScene(const std::string &_in_buffer);
    
    int PackHttpMsg();
    
    void Write2BaseResp(std::string &_resp, size_t _size);
    
    AutoBuffer *GetHttpResp();
    
    SOCKET GetSocket() const;
    
  private:
    static void __ShowHttpHeader(AutoBuffer &_out);
    
  protected:
    SOCKET                              socket_;
    BaseNetSceneResp::BaseNetSceneResp  base_resp_;
    AutoBuffer                          resp_msg_;
    int                                 status_code_;
    std::string                         status_desc_;
    std::map <std::string, std::string> http_headers_;

};


#endif //OI_SVR_NETSCENEBASE_H
