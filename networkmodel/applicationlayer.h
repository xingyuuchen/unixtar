#pragma once
#include "autobuffer.h"


enum TApplicationProtocol {
    kNone = 0,
    kHttp1_1,
    kHttp2_0,
    kHttp3_0,
    kWebSocket,
    kProtocolsMax,
};

const char *const ApplicationProtoToString[kProtocolsMax] = {
        "Application Protocol Unknown",
        "Http 1.1",
        "Http 2.0",
        "Http 3.0",
        "WebSocket",
};


/**
 * Base class for all application protocol packet.
 */
class ApplicationPacket {
  public:
    
    ApplicationPacket();
    
    virtual ~ApplicationPacket();
    
    bool IsLongLink() const;
    
    virtual TApplicationProtocol ApplicationProtocol() const = 0;
    
};


/**
 * Base class for all application protocol parser.
 */
class ApplicationProtocolParser {
  public:
    
    ApplicationProtocolParser(ApplicationPacket *_packet,
                              AutoBuffer *_buff);
    
    virtual ~ApplicationProtocolParser();
    
    /**
     *
     * @return
     */
    virtual int DoParse() = 0;
    
    virtual bool IsErr() const = 0;
    
    virtual bool IsEnd() const = 0;
    
    virtual bool IsUpgradeProtocol() const;
    
    virtual TApplicationProtocol ProtocolUpgradeTo();
    
    virtual void Reset();
    
  protected:
    ApplicationPacket         * application_packet_;
    AutoBuffer                * buffer_;
    
};
