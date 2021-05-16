#include "headerfield.h"
#include "log.h"
#include <cstring>
#include "strutil.h"


namespace http {


const char *const HeaderField::kHost = "Host";
const char *const HeaderField::kContentLength = "Content-Length";
const char *const HeaderField::kContentType = "Content-Type";
const char *const HeaderField::kAccept = "Accept";
const char *const HeaderField::kAcceptEncoding = "Accept-Encoding";
const char *const HeaderField::kUserAgent = "User-Agent";
const char *const HeaderField::kAcceptLanguage = "Accept-Language";
const char *const HeaderField::kTransferEncoding = "Transfer-Encoding";
const char *const HeaderField::kConnection = "Connection";
const char *const HeaderField::kCacheControl = "Cache-Control";
const char *const HeaderField::kAccessControlAllowOrigin = "Access-Control-Allow-Origin";
const char *const HeaderField::kCookie = "Cookie";
const char *const HeaderField::kSetCookie = "Set-Cookie";


const char *const HeaderField::kOctetStream = "application/octet-stream";
const char *const HeaderField::kApplicationJson = "application/json";
const char *const HeaderField::kXWwwFormUrlencoded = "application/x-www-form-urlencoded";
const char *const HeaderField::kTextPlain = "text/plain";
const char *const HeaderField::kTextHtml = "text/html";
const char *const HeaderField::kTextCss = "text/css";
const char *const HeaderField::kImageJpg = "image/jpeg";
const char *const HeaderField::kImagePng = "image/png";
const char *const HeaderField::kConnectionClose = "close";
const char *const HeaderField::kKeepAlive = "keep-alive";
const char *const HeaderField::kConnectionUpgrade = "upgrade";
const char *const HeaderField::kAccessControlOriginAll = "*";
const char *const HeaderField::kTransferChunked = "chunked";


void HeaderField::InsertOrUpdate(const std::string &_key,
                                       const std::string &_value) {
    header_fields_[_key] = _value;
}


size_t HeaderField::HeaderSize() {
    size_t ret = 0;
    for (auto & header_field : header_fields_) {
        ret += header_field.first.size();
        ret += header_field.second.size();
        ret += 4;
    }
    return ret;
}

bool HeaderField::IsKeepAlive() const {
    return __IsConnection(kKeepAlive);
}

bool HeaderField::IsConnectionClose() const {
    return __IsConnection(kConnectionClose);
}

bool HeaderField::IsConnectionUpgrade() const {
    return __IsConnection(kConnectionUpgrade);
}

uint64_t HeaderField::ContentLength() const {
    for (const auto & header_field : header_fields_) {
        if (0 == strcmp(header_field.first.c_str(), kContentLength)) {
            return strtoul(header_field.second.c_str(), nullptr, 10);
        }
    }
    LogI("No such field: %s", kContentLength)
    return 0;
}

void HeaderField::ToString(std::string &_target) {
    _target.clear();
    for (auto & header_field : header_fields_) {
        _target += header_field.first;
        _target += ": ";
        _target += header_field.second;
        _target += "\r\n";
    }
    _target += "\r\n";
}

bool HeaderField::ParseFromString(std::string &_from) {
    std::vector<std::string> headers;
    str::split(_from, "\r\n", headers);
    for (auto & header_str : headers) {
        std::vector<std::string> header_pair;
        str::split(header_str, ": ", header_pair);
        if (header_pair.size() != 2) {
            LogE("err header pair: %s", header_str.c_str())
            return false;
        }
        InsertOrUpdate(header_pair[0], header_pair[1]);
    }
    return true;
}

void HeaderField::Reset() {
    header_fields_.clear();
}

void HeaderField::AppendToBuffer(AutoBuffer &_out_buff) {
    std::string str;
    ToString(str);
    _out_buff.Write(str.data(), str.size());
}

bool HeaderField::__IsConnection(const char *_value) const {
    for (const auto & header_field : header_fields_) {
        if (0 == strcmp(header_field.first.c_str(), kConnection)) {
            return 0 == strcmp(header_field.second.c_str(), _value);
        }
    }
    LogI("No such field: %s", kConnection)
    return false;
}

}
