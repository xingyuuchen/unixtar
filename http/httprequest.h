#ifndef OI_SVR_HTTPREQUSET_H
#define OI_SVR_HTTPREQUSET_H

#include "firstline.h"
#include "headerfield.h"
#include "autobuffer.h"
#include <map>


namespace http { namespace request {


void Pack(const std::string &_host, const std::string &_url,
          const std::map<std::string, std::string> &_headers,
          AutoBuffer &_send_body, AutoBuffer &_out_buff);


class Parser {
  public:
    Parser();
    
    enum TPosition {
        kNone,
        kRequestLine,
        kRequestHeaders,
        kBody,
        kEnd,
        kError,
    };
    
    void DoParse();

    void __ResolveRequestLine();
    
    void __ResolveRequestHeaders();
    
    void __ResolveBody();
    
    bool IsEnd() const;
    
    bool IsErr() const;
    
    TPosition GetPosition() const;
    
    AutoBuffer *GetBody();
    
    AutoBuffer *GetBuff();
    
  private:
    TPosition                               position_;
    http::RequestLine                       request_line_;
    http::HeaderField                       headers_;
    size_t                                  request_line_len_;
    size_t                                  request_header_len_;
    size_t                                  resolved_len_;
    AutoBuffer                              body_;
    AutoBuffer                              buff_;
    
};

}}


#endif //OI_SVR_HTTPREQUSET_H
