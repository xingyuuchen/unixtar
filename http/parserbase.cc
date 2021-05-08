#include "parserbase.h"
#include "log.h"
#include "strutil.h"


http::ParserBase::ParserBase()
        : position_(TPosition::kNone)
        , resolved_len_(0)
        , first_line_len_(0)
        , header_len_(0) {
}

http::ParserBase::~ParserBase() = default;

bool http::ParserBase::IsEnd() const { return position_ == kEnd; }

bool http::ParserBase::IsErr() const { return position_ == kError; }

AutoBuffer *http::ParserBase::GetBuff() { return &buff_; }

char *http::ParserBase::GetBody() {
    return buff_.Ptr(buff_.Length() - GetContentLength());
}

http::ParserBase::TPosition http::ParserBase::GetPosition() const { return position_; }

size_t http::ParserBase::GetContentLength() const {
    size_t content_len = headers_.GetContentLength();
    if (content_len <= 0) {
        LogE("content_len: %zu", content_len)
        return 0;
    }
    return content_len;
}

bool http::ParserBase::_ResolveHeaders() {
    LogI("Resolve Headers")
    char *ret = str::strnstr(buff_.Ptr(resolved_len_),
                             "\r\n\r\n", buff_.Length() - resolved_len_);
    if (!ret) {
        return false;
    }
    
    std::string headers_str(buff_.Ptr(resolved_len_), ret - buff_.Ptr(resolved_len_));
    
    if (headers_.ParseFromString(headers_str)) {
        resolved_len_ += ret - buff_.Ptr(resolved_len_) + 4;  // 4 for \r\n\r\n
        header_len_ = resolved_len_ - first_line_len_;
        position_ = kBody;
        return true;
        
    } else {
        position_ = kError;
        LogE("ParseFromString failed")
        return false;
    }
}

bool http::ParserBase::_ResolveBody() {
    uint64_t content_length = headers_.GetContentLength();
    if (content_length == 0) {
        LogI("content_length = 0")
        position_ = kError;
        return false;
    }
    size_t new_size = buff_.Length() - resolved_len_;
    resolved_len_ += new_size;
    
    size_t curr_body_len = buff_.Length() - first_line_len_ - header_len_;
    if (content_length < curr_body_len) {
        LogI("recv more bytes than Content-Length(%lld)", content_length)
        position_ = kError;
        return false;
    } else if (content_length == curr_body_len) {
        position_ = kEnd;
        return true;
    }
    return false;
}

void http::ParserBase::DoParse() {
    if (buff_.Length() <= 0) {
        LogE("buff_.len: %zd", buff_.Length())
        return;
    }
    size_t unresolved_len = buff_.Length() - resolved_len_;
    if (unresolved_len <= 0) {
        LogI("no bytes need to be resolved: %zd", unresolved_len)
        return;
    }
    
    while (true) {
        bool success = false;
        if (position_ == kNone) {
            if (resolved_len_ == 0 && buff_.Length() > 0) {
                position_ = kFirstLine;
                success = _ResolveFirstLine();
            }
        } else if (position_ == kFirstLine) {
            success = _ResolveFirstLine();
        
        } else if (position_ == kHeaders) {
            success = _ResolveHeaders();
        
        } else if (position_ == kBody) {
            success = _ResolveBody();
        }

        if (position_ == kEnd) {
            return;
        }
        if (position_ == kError) {
            LogI("error occurred, do nothing")
            return;
        }
        if (!success) {
            return;
        }
    }

}

