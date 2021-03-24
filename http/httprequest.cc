#include "httprequest.h"
#include "firstline.h"
#include "headerfield.h"
#include "log.h"
#include <string.h>
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
    header_field.InsertOrUpdate(HeaderField::KHost, _host);
    header_field.InsertOrUpdate(HeaderField::KConnection, HeaderField::KConnectionClose);
    for (auto iter = _headers.begin(); iter != _headers.end(); iter++) {
        header_field.InsertOrUpdate(iter->first, iter->second);
    }
    
    char len_str[9] = {0, };
    snprintf(len_str, sizeof(len_str), "%zu", _send_body.Length());
    header_field.InsertOrUpdate(HeaderField::KContentLength, len_str);
    
    header_field.AppendToBuffer(_out_buff);
    _out_buff.Write(_send_body.Ptr(), _send_body.Length());
    
}



Parser::Parser()
    : position_(TPosition::kNone)
    , resolved_len_(0)
    , request_line_len_(0)
    , request_header_len_(0) {}
        

void Parser::__ResolveRequestLine() {
    LogI(__FILE__, "[__ResolveRequestLine]")
    char *start = buff_.Ptr();
    char *crlf = oi::strnstr(start, "\r\n", buff_.Length());
    if (crlf) {
        std::string req_line(start, crlf - start);
        if (request_line_.ParseFromString(req_line)) {
            position_ = kRequestHeaders;
            resolved_len_ = crlf - start + 2;   // 2 for CRLF
            request_line_len_ = resolved_len_;
    
            if (buff_.Length() > resolved_len_) {
                __ResolveRequestHeaders();
            }
            return;
            
        } else {
            position_ = kError;
        }
    }
    resolved_len_ = 0;
}

void Parser::__ResolveRequestHeaders() {
    LogI(__FILE__, "[__ResolveRequestHeaders]")
    char *ret = oi::strnstr(buff_.Ptr(resolved_len_),
                    "\r\n\r\n", buff_.Length() - resolved_len_);
    if (ret == NULL) { return; }
    
    std::string headers_str(buff_.Ptr(resolved_len_), ret - buff_.Ptr(resolved_len_));
    
    if (headers_.ParseFromString(headers_str)) {
        resolved_len_ += ret - buff_.Ptr(resolved_len_) + 4;  // 4 for \r\n\r\n
        request_header_len_ = resolved_len_ - request_line_len_;
        
        if (buff_.Length() > resolved_len_) {
            position_ = kBody;
            __ResolveBody();
        } else if (request_line_.GetMethod() == http::THttpMethod::kGET) {
            position_ = kEnd;
        }
    } else {
        position_ = kError;
        LogE(__FILE__, "[__ResolveRequestHeaders] headers_.ParseFromString Err")
    }
}

void Parser::__ResolveBody() {
    uint64_t content_length = headers_.GetContentLength();
    if (content_length == 0) {
        LogE(__FILE__, "[__ResolveBody] Content-Length = 0")
        position_ = kError;
        return;
    }
    size_t new_size = buff_.Length() - resolved_len_;
    resolved_len_ += new_size;
    
    size_t curr_body_len = buff_.Length() - request_line_len_ - request_header_len_;
    if (content_length < curr_body_len) {
        LogI(__FILE__, "[__ResolveBody] recv more bytes than"
             " Content-Length(%lld)", content_length)
        position_ = kError;
    } else if (content_length == curr_body_len) {
        position_ = kEnd;
    }

}


void Parser::DoParse() {
    size_t unresolved_len = buff_.Length() - resolved_len_;
    if (unresolved_len <= 0) {
        LogI(__FILE__, "[DoParse] no bytes need to be resolved: %zd", unresolved_len)
        return;
    }
    
    if (position_ == kNone) {
        if (resolved_len_ == 0 && buff_.Length() > 0) {
            position_ = kRequestLine;
            __ResolveRequestLine();
        }
        
    } else if (position_ == kRequestLine) {
        __ResolveRequestLine();
        
    } else if (position_ == kRequestHeaders) {
        __ResolveRequestHeaders();
        
    } else if (position_ == kBody) {
        __ResolveBody();
        
    } else if (position_ == kEnd) {
        LogI(__FILE__, "[DoParse] kEnd")
        
    } else if (position_ == kError) {
        LogI(__FILE__, "[DoParse] error already occurred, do nothing.")
    }
}


bool Parser::IsEnd() const { return position_ == kEnd; }

bool Parser::IsErr() const { return position_ == kError; }

Parser::TPosition Parser::GetPosition() const { return position_; }

AutoBuffer *Parser::GetBuff() { return &buff_; }

AutoBuffer *Parser::GetBody() {
    if (request_line_.GetMethod() == http::THttpMethod::kGET) {
        LogI(__FILE__, "[GetBody] GET, no http body, return NULL")
        return NULL;
    }
    size_t content_len = headers_.GetContentLength();
    if (content_len <= 0) {
        LogE(__FILE__, "[GetBody] content_len <= 0, return NULL")
        return NULL;
    }
    body_.SetPtr(buff_.Ptr(buff_.Length() - content_len));
    body_.SetLength(content_len);
    body_.ShareFromOther(true);
    return &body_;
}

}}

