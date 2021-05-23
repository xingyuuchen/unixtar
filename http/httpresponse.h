#pragma once

#include "httppacket.h"


namespace http { namespace response {

class HttpResponse;

void Pack(http::THttpVersion _http_ver, int _resp_code, const char *_status_desc,
          std::map<std::string, std::string> *_headers,
          AutoBuffer &_out_buff, std::string *_send_body = nullptr);


class HttpResponse : public http::HttpPacket {
  public:
    using Ptr = std::shared_ptr<HttpResponse>;
    
    HttpResponse();
    
    ~HttpResponse() override;
    
    http::StatusLine *getStatusLine();
    
    int StatusCode() const;
    
    THttpVersion Version() const;
    
    std::string &StatusDesc();
    
  private:
    http::StatusLine    status_line_;
};


class Parser : public http::HttpParser {
  public:
    
    Parser(AutoBuffer *_buff, const HttpResponse::Ptr& _http_resp);
    
    ~Parser() override;

  protected:
    bool _ResolveFirstLine() override;

  private:
    http::StatusLine    * status_line_;
    
};

}}

