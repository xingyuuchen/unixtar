#pragma once


enum TApplicationProtocol {
    kUnknown = 0,
    kHttp1_1,
    kHttp2_0,
    kHttp3_0,
    kWebSocket,
};


class ApplicationPacket {
  public:
    
    ApplicationPacket();
    
    virtual ~ApplicationPacket();
    
    bool IsLongLink() const;
    
    virtual TApplicationProtocol ApplicationProtocol() const = 0;
    
};


class ApplicationProtocolParser {
  public:
    
    ApplicationProtocolParser();
    
    /**
     *
     * @return
     */
    virtual int DoParse() = 0;
    
    virtual bool IsErr() const = 0;
    
    virtual bool IsEnd() const = 0;
    
    virtual ~ApplicationProtocolParser();
    
};
