#pragma once

#include "parserbase.h"


namespace http { namespace request {


void Pack(const std::string &_host, const std::string &_url,
          const std::map<std::string, std::string> *_headers,
          AutoBuffer &_send_body, AutoBuffer &_out_buff);


class Parser : public http::ParserBase {
  public:
    Parser();
    
    ~Parser() override;
    
    bool IsMethodPost() const;
    
    char *GetBody() override;
    
    std::string &GetRequestUrl();
    
    size_t GetContentLength() const override;
    
    std::string &GetUrl();
    
    THttpMethod GetMethod() const;
    
    THttpVersion GetVersion() const;
    
    bool IsHttpRequest() const override;
    
  protected:
    bool _ResolveFirstLine() override;
    
    bool _ResolveBody() override;

  private:
    http::RequestLine                       request_line_;
    
};

}}

