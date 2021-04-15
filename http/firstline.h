#ifndef OI_SVR_REQUESTLINE_H
#define OI_SVR_REQUESTLINE_H

#include <string>
#include "autobuffer.h"


namespace http {

enum THttpMethod {
    kUnknownMethod = 0,
    kGET,
    kPOST,   // only support post
    kDELETE,
    // ...
    kMethodMax,
};
const char *const method2string[kMethodMax] = {
        "unknown",
        "GET",
        "POST",
        "DELETE",
};


enum THttpVersion {
    kUnknownVer = 0,
    kHTTP_0_9,
    kHTTP_1_0,
    kHTTP_1_1,   // only support 1.1
    kHTTP_2_0,
    kVersionMax,
};

const char *const version2string[kVersionMax] = {
        "unknown",
        "HTTP/0.9",
        "HTTP/1.0",
        "HTTP/1.1",
        "HTTP/2.0",
};


THttpMethod GetHttpMethod(const std::string &_str);

THttpVersion GetHttpVersion(const std::string &_str);




class RequestLine {
  public:

    RequestLine();
    
    void SetUrl(const std::string &_url);
    
    std::string &GetUrl();
    
    void SetMethod(THttpMethod _method);
    
    THttpMethod GetMethod() const;
    
    THttpVersion GetVersion() const;
    
    void SetVersion(THttpVersion _version);
    
    void ToString(std::string &_target);
    
    bool ParseFromString(std::string &_from);
    
    void AppendToBuffer(AutoBuffer &_buffer);
    

  private:
    THttpMethod     method_;
    std::string     url_;
    THttpVersion    version_;
    
};




class StatusLine {
  public:
    StatusLine();

    void SetVersion(THttpVersion _version);

    void SetStatusCode(int _status_code);

    void SetStatusDesc(std::string &_desc);

    void ToString(std::string &_target);

    bool ParseFromString(std::string &_from);

    void AppendToBuffer(AutoBuffer &_buffer);


  private:
    THttpVersion    version_;
    int             status_code_;
    std::string     status_desc_;

};


}

#endif //OI_SVR_REQUESTLINE_H
