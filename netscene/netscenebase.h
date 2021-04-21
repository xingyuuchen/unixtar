#pragma once
#include <string>
#include <string.h>
#include <map>
#include "socket/unix_socket.h"
#include "autobuffer.h"
#include <atomic>

#define NETSCENE_INIT_START     static std::atomic_flag has_init = ATOMIC_FLAG_INIT; \
                                if (!has_init.test_and_set()) {
#define NETSCENE_INIT_END       }


/**
 * Base class for all NetScenes.
 *
 * Responsible for:
 *      1. Specifying the net scene type;
 *      2. Generating http body.
 * Note:
 *      1. NOT responsible for any network operation(recv, send, etc.);
 *      2. Business logic is implemented by subclasses overriding DoSceneImpl;
 *      3. Refer to NetSceneHelloSvr.cc/.h as an example for detail.
 *      4. Two subclasses: {@class NetSceneProtobuf}, {@class NetSceneProtobuf}.
 */
 
class NetSceneBase {
  public:
    
    NetSceneBase();
    
    virtual ~NetSceneBase();
    
    /**
     * One way to serial/parse customized data structure is protobuf,
     * so the content type of http is set to application/octet-stream.
     * In this case, derived your class from {@class NetSceneProtobuf}.
     *
     * Otherwise, If you want to use other ways to transform data,
     * set _use_protobuf to false, and edit content-type to your own way, by default
     * it is plain-text, then override these two functions: {@link Data()}, {@link Length()}
     * In this case, derived your class from {@class NetSceneCustom}.
     *
     */
    virtual bool IsUseProtobuf() = 0;
    
    
    /**
     *
     * @return: The unique NetScene Type. Declare it outside the framework.
     */
    virtual int GetType() = 0;
    
    /**
     *
     * @return: A new instance of your NetScene.
     */
    virtual NetSceneBase *NewInstance() = 0;
    
    /**
     * If you use your custom message transform way other than protobuf,
     * override these two functions, informing the framework the pointer of
     * your data and how long it is.
     */
    virtual void *Data() = 0;
    
    virtual size_t Length() = 0;
    
    
    /**
     * A NetSceneCustom can choose to bind a url route.
     * This will be used when the request-body does not specify the NetScene type.
     *
     * But NetSceneProtobuf do not, because all information is in http body.
     */
    virtual const char *Route() = 0;
    
    /**
     * Http Content-Type.
     */
    virtual const char *ContentType() = 0;
    
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
    
    
    /**
     * You do not have to care about this.
     * Implemented by {@class NetSceneProtobuf} and {@class NetSceneProtobuf}.
     */
    virtual int DoScene(const std::string &_in_buffer) = 0;
    
    
    /**
     * You do not have to care about this.
     */
    virtual std::string &GetRespBuffer() final;
    
    
  private:
    
    /**
     * Debug only
     */
    static void __ShowHttpHeader(AutoBuffer &_out);
    
    
  protected:
    std::string                         resp_buffer_;
    
};

