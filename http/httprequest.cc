#include "httprequest.h"
#include "firstline.h"
#include "headerfield.h"
#include "log.h"
#include <cstring>
#include "strutil.h"


namespace http { namespace request {


void Pack(const std::string &_host, const std::string &_url, const std::map<std::string, std::string> &_headers,
          AutoBuffer& _send_body, AutoBuffer &_out_buff) {
    _out_buff.Reset();
    
    RequestLine request_line;
    request_line.SetMethod(http::kPOST);
    request_line.SetVersion(http::kHTTP_1_1);
    request_line.SetUrl(_url);
    request_line.AppendToBuffer(_out_buff);
    
    HeaderField header_field;
    header_field.InsertOrUpdate(HeaderField::kHost, _host);
    header_field.InsertOrUpdate(HeaderField::kConnection, HeaderField::kConnectionClose);
    for (const auto & _header : _headers) {
        header_field.InsertOrUpdate(_header.first, _header.second);
    }
    
    char len_str[9] = {0, };
    snprintf(len_str, sizeof(len_str), "%zu", _send_body.Length());
    header_field.InsertOrUpdate(HeaderField::kContentLength, len_str);
    
    header_field.AppendToBuffer(_out_buff);
    _out_buff.Write(_send_body.Ptr(), _send_body.Length());
    
}


Parser::Parser() : http::ParserBase() {
}

Parser::~Parser() = default;


bool Parser::_ResolveFirstLine() {
    LogI("Resolve Request Line")
    char *start = buff_.Ptr();
    char *crlf = str::strnstr(start, "\r\n", buff_.Length());
    if (crlf) {
        std::string req_line(start, crlf - start);
        if (request_line_.ParseFromString(req_line)) {
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


bool Parser::IsMethodPost() const {
    return request_line_.GetMethod() == THttpMethod::kPOST;
}

std::string &Parser::GetRequestUrl() { return request_line_.GetUrl(); }

char *Parser::GetBody() {
    if (request_line_.GetMethod() == http::THttpMethod::kGET) {
        LogI("GET, return NULL")
        return nullptr;
    }
    return ParserBase::GetBody();
}

size_t Parser::GetContentLength() const {
    if (request_line_.GetMethod() == http::THttpMethod::kGET) {
        LogI("GET, return 0")
        return 0;
    }
    return ParserBase::GetContentLength();
}

std::string &Parser::GetUrl() { return request_line_.GetUrl(); }

THttpMethod Parser::GetMethod() const { return request_line_.GetMethod(); }

THttpVersion Parser::GetVersion() const { return request_line_.GetVersion(); }

bool Parser::IsHttpRequest() const { return true; }

bool Parser::_ResolveBody() {
    if (request_line_.GetMethod() == http::THttpMethod::kGET) {
        if (buff_.Length() - resolved_len_ > 0) {
            LogI("GET request has body")
            position_ = kError;
            return false;
        }
        position_ = kEnd;
        return true;
    }
    return ParserBase::_ResolveBody();
}

}}

