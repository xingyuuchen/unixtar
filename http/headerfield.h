#pragma once

#include <map>
#include <string>
#include "autobuffer.h"



namespace http {


class HeaderField {
  public:
    
    // fields
    static const char *const kHost;
    static const char *const kContentLength;
    static const char *const kContentType;
    static const char *const kAccept;
    static const char *const kAcceptEncoding;
    static const char *const kUserAgent;
    static const char *const kAcceptLanguage;
    static const char *const kTransferEncoding;
    static const char *const kConnection;
    static const char *const kCacheControl;
    static const char *const kAccessControlAllowOrigin;
    static const char *const kCookie;
    static const char *const kSetCookie;
    static const char *const kUpgrade;
    static const char *const kSecWebSocketVersion;
    static const char *const kSecWebSocketKey;
    static const char *const kSecWebSocketAccept;
    static const char *const kSecWebSocketExtensions;
    static const char *const kWebSocketLocation;
    
    // values
    static const char *const kOctetStream;
    static const char *const kTextPlain;
    static const char *const kTextHtml;
    static const char *const kTextCss;
    static const char *const kApplicationJson;
    static const char *const kXWwwFormUrlencoded;
    static const char *const kImageJpg;
    static const char *const kImagePng;
    static const char *const kConnectionClose;
    static const char *const kKeepAlive;
    static const char *const kConnectionUpgrade;
    static const char *const kAccessControlOriginAll;
    static const char *const kTransferChunked;
    static const char *const kWebSocket;
    static const char *const kSecWebSocketVersion13;
    
    
    void InsertOrUpdate(const std::string &_key, const std::string &_value);
    
    const char *Get(const char *_field) const;
    
    uint64_t ContentLength() const;
    
    bool IsKeepAlive() const;
    
    bool IsConnectionClose() const;
    
    bool IsConnectionUpgrade() const;
    
    void AppendToBuffer(AutoBuffer &_out_buff);

    size_t HeaderSize();
    
    void ToString(std::string &_target);
    
    std::map<std::string, std::string> &AsMap();
    
    bool ParseFromString(std::string &_from);
    
    void Reset();

  private:
    bool __IsConnection(const char *_value) const;
    
  private:
    std::map<std::string, std::string>  header_fields_;

};

}

