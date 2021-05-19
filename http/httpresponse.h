#pragma once

#include "httppacket.h"


namespace http { namespace response {

class HttpResponse;

void Pack(http::THttpVersion _http_ver, int _resp_code, const char *_status_desc,
          std::map<std::string, std::string> *_headers,
          AutoBuffer &_out_buff, std::string *_send_body = nullptr);


class Parser : public http::HttpParser {
  public:
    
    Parser(AutoBuffer *_buff, HttpResponse *_http_resp);
    
    ~Parser() override;

  protected:
    bool _ResolveFirstLine() override;

  private:
    http::StatusLine    * status_line_;
    
};


class HttpResponse : public http::HttpPacket {
  public:
    HttpResponse();
    
    ~HttpResponse() override;
    
    http::StatusLine *getStatusLine();
    
    int StatusCode() const;
    
    THttpVersion Version() const;
    
    std::string &StatusDesc();
    
  private:
    http::StatusLine    status_line_;
};

}}

