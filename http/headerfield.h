#ifndef OI_SVR_HEADERFIELD_H
#define OI_SVR_HEADERFIELD_H

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
    static const char *const kAccessControlOriginAll;
    static const char *const kTransferChunked;
    
    
    void InsertOrUpdate(const std::string &_key, const std::string &_value);
    
    uint64_t ContentLength() const;
    
    bool IsKeepAlive() const;
    
    void AppendToBuffer(AutoBuffer &_out_buff);

    size_t GetHeaderSize();
    
    void ToString(std::string &_target);
    
    bool ParseFromString(std::string &_from);
    
    void Reset();
    
  private:
    std::map<std::string, std::string>  header_fields_;

};

}

#endif //OI_SVR_HEADERFIELD_H
