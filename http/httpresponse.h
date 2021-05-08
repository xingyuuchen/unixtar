#pragma once

#include "parserbase.h"


namespace http { namespace response {


void Pack(http::THttpVersion _http_ver, int _resp_code, std::string &_status_desc,
          std::map<std::string, std::string> &_headers,
          AutoBuffer &_out_buff, std::string &_send_body);


class Parser : public http::ParserBase {
  public:
    
    Parser();
    
    ~Parser() override;
    
    int GetStatusCode() const;
    
    THttpVersion GetVersion() const;
    
    std::string &StatusDesc();
    
    bool IsHttpRequest() const override;

  protected:
    bool _ResolveFirstLine() override;

  private:
    http::StatusLine                        status_line_;
    
};

}}

