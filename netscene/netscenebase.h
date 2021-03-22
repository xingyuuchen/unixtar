#ifndef OI_SVR_NETSCENEBASE_H
#define OI_SVR_NETSCENEBASE_H
#include <string>
#include <string.h>
#include <map>
#include "socket/unix_socket.h"
#include "autobuffer.h"
#include "autogen/basenetsceneresp.pb.h"
#include <atomic>

#define NETSCENE_INIT_START     static std::atomic_flag has_init = ATOMIC_FLAG_INIT; \
                                if (!has_init.test_and_set()) {
#define NETSCENE_INIT_END       }


/**
 * Base class for all NetScenes.
 *
 * Responsible for:
 *      1. Specifying the net scene type;
 *      2. Packing http body into http message;
 * Note:
 *      1. NOT responsible for any network operation(recv, send, etc.);
 *      2. Business logic is Implemented the by subclasses by overriding DoSceneImpl.
 *      3. Refer to NetSceneHelloSvr.cc/.h as an example for detail.
 */
 
class NetSceneBase {
  public:
    
    /**
     * The default way to serial/parse customized data structure is by protobuf,
     * so the content type of http is set to application/octet-stream.
     *
     * Otherwise, If you want to use other ways to transform data,
     * set _use_protobuf to false, and edit content-type to your own way, by default
     * it is plain-text, And override these two functions: {@link Data()}, {@link Length()}
     *
     */
    explicit NetSceneBase(bool _use_protobuf = true);
    
    /**
     * If you use your custom message transform way other than protobuf,
     * override these two functions, informing the framework the pointer of
     * your data and how long it is.
     */
    virtual void *Data() { return nullptr; }
    virtual size_t Length() { return 0; }
    
    virtual ~NetSceneBase() { }
    
    /**
     *
     * @return: The Unique NetScene Type. Declare it outside the framework.
     */
    virtual int GetType() = 0;
    
    /**
     *
     * @return: A new instance of a net scene.
     */
    virtual NetSceneBase *NewInstance() = 0;
    
    /**
     *
     * It is Derived classes' responsibility to implement your business logic.
     *
     * @param _in_buffer: The buffer used to parse to your custom data structure.
     * @return: 0 upon successful completion, otherwise non-zero.
     *          No matter what it is, the framework will use {@link GetRespMessage}
     *          to populate http response.
     */
    virtual int DoSceneImpl(const std::string &_in_buffer) = 0;
    
    using RespMessage = google::protobuf::Message;
    /**
     * @return: Your response to front-end, the framework will use it
     *          to populate the http response.
     */
    virtual RespMessage *GetRespMessage() = 0;
    
    virtual void SetSocket(SOCKET _socket) final;
    
    virtual int DoScene(const std::string &_in_buffer) final;
    
    AutoBuffer *GetHttpResp();
    
    virtual SOCKET GetSocket() const final;
    
    
  private:
    int __PackHttpMsg();
    
    /**
     * Debug only
     */
    static void __ShowHttpHeader(AutoBuffer &_out);
    
  protected:
    SOCKET                              socket_;
    int                                 errcode_;
    std::string                         errmsg_;
    
  private:
    BaseNetSceneResp::BaseNetSceneResp  base_resp_;
    AutoBuffer                          http_resp_msg_;
    bool                                use_protobuf_;
    
    // http fields
    int                                 status_code_;
    std::string                         status_desc_;
    std::map <std::string, std::string> http_headers_;

};


#endif //OI_SVR_NETSCENEBASE_H
