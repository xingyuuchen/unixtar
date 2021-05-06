#include "headerfield.h"
#include "log.h"
#include <cstring>
#include "strutil.h"


namespace http {


const char *const HeaderField::kHost = "Host";
const char *const HeaderField::kContentLength = "Content-Length";
const char *const HeaderField::kContentType = "Content-Type";
const char *const HeaderField::kTransferEncoding = "Transfer-Encoding";
const char *const HeaderField::kConnection = "Connection";
const char *const HeaderField::kAccessControlAllowOrigin = "Access-Control-Allow-Origin";
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
const char *const HeaderField::kAccessControlOriginAll = "*";
const char *const HeaderField::kTransferChunked = "chunked";


void HeaderField::InsertOrUpdate(const std::string &_key,
                                       const std::string &_value) {
    header_fields_[_key] = _value;
}


size_t HeaderField::GetHeaderSize() {
    size_t ret = 0;
    for (auto & header_field : header_fields_) {
        ret += header_field.first.size();
        ret += header_field.second.size();
        ret += 4;
    }
    return ret;
}

uint64_t HeaderField::GetContentLength() const {
    for (const auto & header_field : header_fields_) {
        if (0 == strcmp(header_field.first.c_str(), kContentLength)) {
            return strtoul(header_field.second.c_str(), NULL, 10);
        }
    }
    LogI("No such field: Content-Length")
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

void HeaderField::AppendToBuffer(AutoBuffer &_out_buff) {
    std::string str;
    ToString(str);
    _out_buff.Write(str.data(), str.size());
}


}
