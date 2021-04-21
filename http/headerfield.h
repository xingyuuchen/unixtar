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
    static const char *const kTransferEncoding;
    static const char *const kConnection;
    
    // values
    static const char *const kOctetStream;
    static const char *const kTextPlain;
    static const char *const kTextHtml;
    static const char *const kTextCss;
    static const char *const kConnectionClose;
    
    
    void InsertOrUpdate(const std::string &_key, const std::string &_value);
    
    uint64_t GetContentLength() const;
    
    void AppendToBuffer(AutoBuffer &_out_buff);

    size_t GetHeaderSize();
    
    void ToString(std::string &_target);
    
    bool ParseFromString(std::string &_from);
    
    
  private:
    std::map<std::string, std::string>  header_fields_;

};

}

#endif //OI_SVR_HEADERFIELD_H
