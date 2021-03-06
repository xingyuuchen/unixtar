#pragma once

#include "httppacket.h"


namespace http { namespace request {

class HttpRequest;

void Pack(const std::string &_host, const std::string &_url,
          const std::map<std::string, std::string> *_headers,
          std::string &_send_body, AutoBuffer &_out_buff);


class HttpRequest : public http::HttpPacket {
  public:
    using Ptr = std::shared_ptr<HttpRequest>;
    
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


class Parser : public http::HttpParser {
  public:
    
    Parser(AutoBuffer *_buff, const HttpRequest::Ptr& _http_request);
    
    ~Parser() override;
    
    bool IsUpgradeProtocol() const override;
    
    TApplicationProtocol ProtocolUpgradeTo() override;

  protected:
    bool _ResolveFirstLine() override;
    
    bool _ResolveHeaders() override;
    
    bool _ResolveBody() override;

  private:
    http::RequestLine                     * request_line_;
    bool                                    is_upgrade_to_ws_;
};

}}

