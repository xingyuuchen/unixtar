#ifndef OI_SVR_HTTPRESPONSE_H
#define OI_SVR_HTTPRESPONSE_H

#include <map>
#include "autobuffer.h"
#include "firstline.h"
#include "headerfield.h"



namespace http { namespace response {


void Pack(http::THttpVersion _http_ver, int _resp_code, std::string &_status_desc,
          std::map<std::string, std::string> _headers,
          AutoBuffer &_out_buff, AutoBuffer &_send_body);


class Parser {
  public:
    enum TPosition {
        kNone,
        kStatusLine,
        kResponseHeaders,
        kBody,
        kEnd,
        kError,
    };
    
    Parser(AutoBuffer *_body);
    
    void Recv(AutoBuffer &_buff);
    
    bool IsEnd() const;
    
    bool IsErr() const;
    
    TPosition GetPosition() const;
    
    AutoBuffer *GetBody();

  private:
    void __ResolveStatusLine(AutoBuffer &_buff);
    
    void __ResolveResponseHeaders(AutoBuffer &_buff);
    
    void __ResolveBody(AutoBuffer &_buff);
    
  private:
    TPosition                               position_;
    http::StatusLine                        status_line_;
    http::HeaderField                       headers_;
    size_t                                  status_line_len_;      // debug only
    size_t                                  response_header_len_;    // debug only
    size_t                                  resolved_len_;
    AutoBuffer*                             body_;
    
};

}}




#endif //OI_SVR_HTTPRESPONSE_H
