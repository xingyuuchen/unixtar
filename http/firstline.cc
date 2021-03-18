#include "firstline.h"
#include <string.h>
#include "log.h"
#include "strutil.h"


namespace http {


    
THttpMethod GetHttpMethod(const std::string &_str) {
    for (int i = 0; i < kMethodMax; i++) {
        if (strcmp(_str.c_str(), method2string[i]) == 0) {
            return (THttpMethod) i;
        }
    }
    return kUnknownMethod;
}

THttpVersion GetHttpVersion(const std::string &_str) {
    for (int i = 0; i < kVersionMax; ++i) {
        if (strcmp(_str.c_str(), version2string[i]) == 0) {
            return (THttpVersion) i;
        }
    }
    return kUnknownVer;
}



RequestLine::RequestLine()
        : method_(kPOST)
        , version_(kHTTP_1_1)
        , url_() {}

    
void RequestLine::SetMethod(THttpMethod _method) { method_ = _method; }

void RequestLine::SetUrl(const std::string &_url) { url_ = _url; }

void RequestLine::SetVersion(THttpVersion _version) { version_ = _version; }

void RequestLine::ToString(std::string &_target) {
    _target.clear();
    _target += method2string[method_];
    _target += " ";
    _target += url_;
    _target += " ";
    _target += version2string[version_];
    _target += "\r\n";
}

bool RequestLine::ParseFromString(std::string &_from) {
    std::vector<std::string> res;
    oi::split(_from, " ", res);
    if (res.size() != 3) {
        LogI(__FILE__, "[ParseFromString] res.size(): %zd", res.size())
        return false;
    }
    method_ = GetHttpMethod(res[0]);
    url_ = res[1];
    version_ = GetHttpVersion(res[2]);
    return true;
}

void RequestLine::AppendToBuffer(AutoBuffer &_buffer) {
    std::string str;
    ToString(str);
    _buffer.Write(str.data(), str.size());
}

THttpMethod RequestLine::GetMethod() const { return method_; }

THttpVersion RequestLine::GetVersion() const { return version_; }



StatusLine::StatusLine()
    : status_code_(200)
    , version_(kHTTP_1_1)
    , status_desc_() {}


void StatusLine::SetVersion(THttpVersion _version) { version_ = _version; }

void StatusLine::SetStatusCode(int _status_code) { status_code_ = _status_code; }

void StatusLine::SetStatusDesc(std::string &_desc) { status_desc_ = _desc; }

void StatusLine::ToString(std::string &_target) {
    _target.clear();
    _target += version2string[version_];
    _target += " ";
    char status_code_str[4] = {0, };
    snprintf(status_code_str, sizeof(status_code_str), "%u", status_code_);
    _target += status_code_str;
    _target += " ";
    _target += status_desc_;
    _target += "\r\n";
}

bool StatusLine::ParseFromString(std::string &_from) {
    std::vector<std::string> res;
    oi::split(_from, " ", res);
    if (res.size() != 3) {
        LogI(__FILE__, "[ParseFromString] res.size(): %zd", res.size())
        return false;
    }
    version_ = GetHttpVersion(res[0]);
    status_desc_ = GetHttpVersion(res[2]);
    
    status_code_ = std::stoi(res[1]);
    return !(status_code_ < 0 || status_code_ > 1000);
}

void StatusLine::AppendToBuffer(AutoBuffer &_buffer) {
    std::string str;
    ToString(str);
    _buffer.Write(str.data(), str.size());
}


}
