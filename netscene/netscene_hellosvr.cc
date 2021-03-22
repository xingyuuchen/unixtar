#include "netscene_hellosvr.h"
#include "headerfield.h"
#include "constantsprotocol.h"


NetSceneHelloSvr::NetSceneHelloSvr()
        : NetSceneBase() {
    
    // Do Initialization
}

NetSceneBase::RespMessage *NetSceneHelloSvr::GetRespMessage() { return &resp_; }

NetSceneBase *NetSceneHelloSvr::NewInstance() { return new NetSceneHelloSvr(); }


/**
 * The NetSceneType is the unique key to distinguish different NetScenes
 */
int NetSceneHelloSvr::GetType() { return kNetSceneTypeHelloSvr; }


int NetSceneHelloSvr::DoSceneImpl(const std::string &_in_buffer) {
    
    // First, parse the require protobuf massage from _in_buffer.
    NetSceneHelloSvrProto::NetSceneHelloSvrReq req;
    req.ParseFromArray(_in_buffer.data(), _in_buffer.size());
    
    
    // Then obtain data from the pb message.
    uint32_t req_data0 = req.req_data0();
    std::string req_data1 = req.req_data1();
    
    
    // Do something using your data...
    // ....
    // ....
    
    
    // Then populate response pb message, as follows:
    resp_.set_resp_data0(true);
    resp_.add_resp_data1(2.0);
    resp_.add_resp_data1(12.3);
    
    
    // Just get your business done.
    // Don't worry about anything else such as:
    //          1. network transmission;
    //          2. data sending and receiving;
    //          3. etc...
    // They are the framework's responsibility.
    return 0;
}
