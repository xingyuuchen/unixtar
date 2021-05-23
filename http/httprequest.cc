#include "httprequest.h"
#include "firstline.h"
#include "headerfield.h"
#include "log.h"
#include <cstring>
#include <cassert>
#include "strutil.h"


namespace http { namespace request {


void Pack(const std::string &_host, const std::string &_url,
          const std::map<std::string, std::string> *_headers,
          std::string &_send_body, AutoBuffer &_out_buff) {
    _out_buff.Reset();
    
    RequestLine request_line;
    request_line.SetMethod(http::kPOST);
    request_line.SetVersion(http::kHTTP_1_1);
    request_line.SetUrl(_url);
    request_line.AppendToBuffer(_out_buff);
    
    HeaderField header_field;
    header_field.InsertOrUpdate(HeaderField::kHost, _host);
    header_field.InsertOrUpdate(HeaderField::kConnection, HeaderField::kConnectionClose);
    if (_headers) {
        for (const auto & header : *_headers) {
            header_field.InsertOrUpdate(header.first, header.second);
        }
    }
    
    char len_str[9] = {0, };
    snprintf(len_str, sizeof(len_str), "%zu", _send_body.size());
    header_field.InsertOrUpdate(HeaderField::kContentLength, len_str);
    
    header_field.AppendToBuffer(_out_buff);
    _out_buff.Write(_send_body.data(), _send_body.size());
    
}


Parser::Parser(AutoBuffer *_buff, const http::request::HttpRequest::Ptr& _http_request)
        : http::HttpParser(_http_request, _buff)
        , request_line_(_http_request->GetRequestLine())
        , is_upgrade_to_ws_(false) {
    
    assert(request_line_);
}

Parser::~Parser() = default;

bool Parser::IsUpgradeProtocol() const {
    return is_upgrade_to_ws_;
}

TApplicationProtocol Parser::ProtocolUpgradeTo() {
    if (is_upgrade_to_ws_) {
        return TApplicationProtocol::kWebSocket;
    }
    return ApplicationProtocolParser::ProtocolUpgradeTo();
}

bool Parser::_ResolveFirstLine() {
    LogI("Resolve Request Line")
    char *start = buffer_->Ptr();
    char *crlf = str::strnstr(start, "\r\n", buffer_->Length());
    if (crlf) {
        std::string req_line(start, crlf - start);
        if (request_line_->ParseFromString(req_line)) {
            position_ = kHeaders;
            resolved_len_ = crlf - start + 2;   // 2 for CRLF
            first_line_len_ = resolved_len_;
            return true;
            
        } else {
            position_ = kError;
            return false;
        }
    }
    position_ = kFirstLine;
    resolved_len_ = 0;
    return false;
}

bool Parser::_ResolveHeaders() {
    if (HttpParser::_ResolveHeaders()) {
        if (headers_->IsConnectionUpgrade()) {
            is_upgrade_to_ws_ = true;
        }
        return true;
    }
    return false;
}

bool Parser::_ResolveBody() {
    if (request_line_->GetMethod() == http::THttpMethod::kGET) {
        if (buffer_->Length() - resolved_len_ > 0) {
            LogI("GET request has body")
            position_ = kError;
            return false;
        }
        position_ = kEnd;
        return true;
    }
    return HttpParser::_ResolveBody();
}



HttpRequest::HttpRequest()
        : http::HttpPacket() {
    
}

AutoBuffer *HttpRequest::Body() {
    if (request_line_.GetMethod() == http::THttpMethod::kGET) {
        LogD("GET, return NULL")
        return nullptr;
    }
    return HttpPacket::Body();
}


size_t HttpRequest::ContentLength() const {
    if (request_line_.GetMethod() == http::THttpMethod::kGET) {
        LogD("GET, return 0")
        return 0;
    }
    return HttpPacket::ContentLength();
}

std::string &HttpRequest::Url() { return request_line_.GetUrl(); }

THttpMethod HttpRequest::Method() const { return request_line_.GetMethod(); }

bool HttpRequest::IsMethodPost() const { return Method() == THttpMethod::kPOST; }

THttpVersion HttpRequest::Version() const { return request_line_.GetVersion(); }

http::RequestLine *HttpRequest::GetRequestLine() { return &request_line_; }

HttpRequest::~HttpRequest() = default;

}}

