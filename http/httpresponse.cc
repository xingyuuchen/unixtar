#include "httpresponse.h"
#include "strutil.h"
#include "log.h"


namespace http { namespace response {


void Pack(http::THttpVersion _http_ver, int _resp_code, std::string &_status_desc,
            std::map<std::string, std::string> _headers,
            AutoBuffer &_out_buff, std::string &_send_body) {

    _out_buff.Reset();
    
    http::StatusLine status_line;
    status_line.SetStatusCode(_resp_code);
    status_line.SetStatusDesc(_status_desc);
    status_line.SetVersion(_http_ver);
    status_line.AppendToBuffer(_out_buff);
    
    HeaderField header_field;
    for (auto iter = _headers.begin(); iter != _headers.end(); iter++) {
        header_field.InsertOrUpdate(iter->first, iter->second);
    }
    
    char len_str[9] = {0, };
    snprintf(len_str, sizeof(len_str), "%zu", _send_body.size());
    header_field.InsertOrUpdate(HeaderField::KContentLength, len_str);
    
    header_field.AppendToBuffer(_out_buff);
    _out_buff.Write(_send_body.data(), _send_body.size());

}



Parser::Parser(AutoBuffer *_body)
    : resolved_len_(0)
    , body_(_body)
    , position_(kNone)
    , status_line_len_(0)
    , response_header_len_(0) {}
    

AutoBuffer * Parser::GetBody() { return body_; }

bool Parser::IsErr() const { return position_ == kError; }

bool Parser::IsEnd() const { return position_ == kEnd; }

Parser::TPosition Parser::GetPosition() const { return position_; }


void Parser::__ResolveStatusLine(AutoBuffer &_buff) {
    char *start = _buff.Ptr();
    char *crlf = oi::strnstr(start, "\r\n", _buff.Length());
    if (crlf != NULL) {
        std::string req_line(start, crlf - start);
        if (status_line_.ParseFromString(req_line)) {
            position_ = kResponseHeaders;
            resolved_len_ = crlf - start + 2;   // 2 for CRLF
            status_line_len_ = resolved_len_;
            
            if (_buff.Length() > resolved_len_) {
                __ResolveResponseHeaders(_buff);
            }
            return;
            
        } else {
            position_ = kError;
        }
    }
    resolved_len_ = 0;
}

void Parser::__ResolveResponseHeaders(AutoBuffer &_buff) {
    char *ret = oi::strnstr(_buff.Ptr(resolved_len_),
                            "\r\n\r\n", _buff.Length() - resolved_len_);
    if (ret == NULL) { return; }
    
    std::string headers_str(_buff.Ptr(resolved_len_), ret - _buff.Ptr(resolved_len_));
    
    if (headers_.ParseFromString(headers_str)) {
        resolved_len_ += ret - _buff.Ptr(resolved_len_) + 4;  // 4 for \r\n\r\n
        response_header_len_ = resolved_len_ - status_line_len_;
        position_ = kBody;
        
        if (_buff.Length() > resolved_len_) {
            __ResolveBody(_buff);
        }
    } else {
        position_ = kError;
        LogE(__FILE__, "[__ResolveResponseHeaders] headers_.ParseFromString Err")
    }
}

void Parser::__ResolveBody(AutoBuffer &_buff) {
    uint64_t content_length = headers_.GetContentLength();
    if (content_length == 0) {
        LogI(__FILE__, "[__ResolveBody] content_length = 0")
        position_ = kError;
        return;
    }
    size_t new_size = _buff.Length() - resolved_len_;
    body_->Write(_buff.Ptr(resolved_len_), new_size);
    resolved_len_ += new_size;
    
    if (content_length < body_->Length()) {
        LogI(__FILE__, "[__ResolveBody] recv more %zd bytes than Content-Length(%lld)",
             body_->Length(), content_length)
        position_ = kError;
    } else if (content_length == body_->Length()) {
        position_ = kEnd;
    }
}

void Parser::Recv(AutoBuffer &_buff) {
    if (_buff.Length() <= 0) { return; }
    size_t unresolved_len = _buff.Length() - resolved_len_;
    if (unresolved_len <= 0) {
        LogI(__FILE__, "[resp::Parser::Recv] no bytes need to be resolved: %zd", unresolved_len)
        return;
    }
    
    if (position_ == kNone) {
        LogI(__FILE__, "[Recv] kNone")
        if (resolved_len_ == 0 && _buff.Length() > 0) {
            position_ = kStatusLine;
            __ResolveStatusLine(_buff);
        }
        
    } else if (position_ == kStatusLine) {
        LogI(__FILE__, "[Recv] kRequestLine")
        __ResolveStatusLine(_buff);
        
    } else if (position_ == kResponseHeaders) {
        LogI(__FILE__, "[Recv] kRequestHeaders")
        __ResolveResponseHeaders(_buff);
        
    } else if (position_ == kBody) {
        __ResolveBody(_buff);
        
    } else if (position_ == kEnd) {
        LogI(__FILE__, "[Recv] kEnd")
        
    } else if (position_ == kError) {
        LogI(__FILE__, "[Recv] error already occurred, do nothing.")
    }
}


}}
