#ifndef OI_SVR_HEADERFIELD_H
#define OI_SVR_HEADERFIELD_H

#include <map>
#include <string>
#include "autobuffer.h"



namespace http {


class HeaderField {
  public:
    
    // fields
    static const char *const KHost;
    static const char *const KContentLength;
    static const char *const KContentType;
    static const char *const KTransferEncoding;
    static const char *const KConnection;
    
    // values
    static const char *const KOctetStream;
    static const char *const KPlainText;
    static const char *const KConnectionClose;
    
    
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
