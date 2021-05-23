#pragma once
#include "autobuffer.h"
#include <memory>


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
    using Ptr = std::shared_ptr<ApplicationPacket>;
    
    ApplicationPacket();
    
    virtual ~ApplicationPacket();
    
    bool IsLongLink() const;
    
    virtual TApplicationProtocol Protocol() const = 0;
    
    virtual Ptr AllocNewPacket();
    
};


/**
 * Base class for all application protocol parser.
 */
class ApplicationProtocolParser {
  public:
    
    ApplicationProtocolParser(ApplicationPacket::Ptr _packet,
                              AutoBuffer *_buff);
    
    virtual ~ApplicationProtocolParser();
    
    void SetPacketToParse(ApplicationPacket::Ptr _packet);
    
    /**
     *
     * @return
     */
    virtual int DoParse() = 0;
    
    virtual bool IsErr() const = 0;
    
    virtual bool IsEnd() const = 0;
    
    virtual bool IsUpgradeProtocol() const;
    
    virtual TApplicationProtocol ProtocolUpgradeTo();
    
    virtual void OnApplicationPacketChanged(
            const ApplicationPacket::Ptr& _new);
    
    virtual void Reset();
    
  protected:
    ApplicationPacket::Ptr      application_packet_;
    AutoBuffer                * buffer_;
    
};
