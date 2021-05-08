#include "httpresponse.h"
#include "strutil.h"
#include "log.h"


namespace http { namespace response {


void Pack(http::THttpVersion _http_ver, int _resp_code, std::string &_status_desc,
            std::map<std::string, std::string> &_headers,
            AutoBuffer &_out_buff, std::string &_send_body) {

    _out_buff.Reset();
    
    http::StatusLine status_line;
    status_line.SetStatusCode(_resp_code);
    status_line.SetStatusDesc(_status_desc);
    status_line.SetVersion(_http_ver);
    status_line.AppendToBuffer(_out_buff);
    
    HeaderField header_field;
    for (auto & header : _headers) {
        header_field.InsertOrUpdate(header.first, header.second);
    }
    
    char len_str[9] = {0, };
    snprintf(len_str, sizeof(len_str), "%zu", _send_body.size());
    header_field.InsertOrUpdate(HeaderField::kContentLength, len_str);
    
    header_field.AppendToBuffer(_out_buff);
    _out_buff.Write(_send_body.data(), _send_body.size());

}



Parser::Parser() : http::ParserBase() {
}

Parser::~Parser() = default;


bool Parser::_ResolveFirstLine() {
    LogI("Resolve Status Line")
    char *start = buff_.Ptr();
    char *crlf = str::strnstr(start, "\r\n", buff_.Length());
    if (crlf) {
        std::string req_line(start, crlf - start);
        if (status_line_.ParseFromString(req_line)) {
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

int Parser::GetStatusCode() const { return status_line_.StatusCode(); }

THttpVersion Parser::GetVersion() const { return status_line_.GetVersion(); }

std::string &Parser::StatusDesc() { return status_line_.StatusDesc(); }

bool Parser::IsHttpRequest() const { return false; }

}}
