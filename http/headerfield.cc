#include "headerfield.h"
#include "log.h"
#include <string.h>
#include "strutil.h"


namespace http {


const char *const HeaderField::KHost = "Host";
const char *const HeaderField::KContentLength = "Content-Length";
const char *const HeaderField::KContentType = "Content-Type";
const char *const HeaderField::KTransferEncoding = "Content-Length";
const char *const HeaderField::KConnection = "Connection";


const char *const HeaderField::KOctetStream = "application/octet-stream";
const char *const HeaderField::KPlainText = "plain-text";
const char *const HeaderField::KConnectionClose = "close";


void HeaderField::InsertOrUpdate(const std::string &_key,
                                       const std::string &_value) {
    header_fields_[_key] = _value;
}


size_t HeaderField::GetHeaderSize() {
    size_t ret = 0;
    for (auto entry = header_fields_.begin(); entry != header_fields_.end(); entry++) {
        ret += entry->first.size();
        ret += entry->second.size();
        ret += 4;
    }
    return ret;
}

uint64_t HeaderField::GetContentLength() const {
    for (const auto & header_field : header_fields_) {
        if (0 == strcmp(header_field.first.c_str(), KContentLength)) {
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
    oi::split(_from, "\r\n", headers);
    for (auto & header_str : headers) {
        std::vector<std::string> header_pair;
        oi::split(header_str, ": ", header_pair);
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
