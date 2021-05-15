#pragma once

#include "httppacket.h"


namespace http { namespace request {

class HttpRequest;

void Pack(const std::string &_host, const std::string &_url,
          const std::map<std::string, std::string> *_headers,
          std::string &_send_body, AutoBuffer &_out_buff);


class Parser : public http::HttpParser {
  public:
    
    Parser(AutoBuffer *_buff, HttpRequest *_http_request);
    
    ~Parser() override;
    
  protected:
    bool _ResolveFirstLine() override;
    
    bool _ResolveBody() override;

  private:
    http::RequestLine                     * request_line_;
};



class HttpRequest : public http::HttpPacket {
  public:
    HttpRequest();
    
    ~HttpRequest() override;
    
    AutoBuffer *Body() override;
    
    size_t ContentLength() const override;
    
    http::RequestLine *GetRequestLine();
    
    std::string &Url();
    
    THttpMethod Method() const;
    
    bool IsMethodPost() const;
    
    THttpVersion Version() const;
    
  private:
    http::RequestLine   request_line_;
};

}}

