#include "httppacket.h"
#include "log.h"
#include "strutil.h"
#include <cassert>



http::HttpPacket::HttpPacket() {

}

http::HeaderField *http::HttpPacket::Headers() { return &headers_; }

AutoBuffer *http::HttpPacket::Body() {
    return &body_;
//    return buffer_.Ptr(buffer_.Length() - ContentLength());
}

TApplicationProtocol http::HttpPacket::ApplicationProtocol() const {
    return kHttp1_1;
}

size_t http::HttpPacket::ContentLength() const {
    size_t content_len = headers_.ContentLength();
    if (content_len <= 0) {
        LogE("content_len: %zu", content_len)
        return 0;
    }
    return content_len;
}

http::HttpPacket::~HttpPacket() {

}

void http::HttpPacket::SetBody(char *_ptr, size_t _length) {
    body_.ShallowCopyFrom(_ptr, _length);
}


http::HttpParser::HttpParser(http::HeaderField *_headers,
                             AutoBuffer *_buff)
        : ApplicationProtocolParser()
        , position_(TPosition::kNone)
        , headers_(_headers)
        , buffer_(_buff)
        , resolved_len_(0)
        , first_line_len_(0)
        , header_len_(0) {
    
    assert(headers_ && buffer_);
}

http::HttpParser::~HttpParser() = default;

bool http::HttpParser::IsEnd() const { return position_ == kEnd; }

bool http::HttpParser::IsErr() const { return position_ == kError; }



http::HttpParser::TPosition http::HttpParser::GetPosition() const { return position_; }

AutoBuffer *http::HttpParser::Buffer() { return buffer_; }


bool http::HttpParser::_ResolveHeaders() {
    LogI("Resolve Headers")
    char *ret = str::strnstr(buffer_->Ptr(resolved_len_),
                             "\r\n\r\n", buffer_->Length() - resolved_len_);
    if (!ret) {
        return false;
    }
    
    std::string headers_str(buffer_->Ptr(resolved_len_),
                            ret - buffer_->Ptr(resolved_len_));
    
    if (headers_->ParseFromString(headers_str)) {
        resolved_len_ += ret - buffer_->Ptr(resolved_len_) + 4;  // 4 for \r\n\r\n
        header_len_ = resolved_len_ - first_line_len_;
        position_ = kBody;
        return true;
        
    } else {
        position_ = kError;
        LogE("ParseFromString failed")
        return false;
    }
}

bool http::HttpParser::_ResolveBody() {
    uint64_t content_length = headers_->ContentLength();
    if (content_length == 0) {
        LogI("content_length = 0")
        position_ = kError;
        return false;
    }
    size_t new_size = buffer_->Length() - resolved_len_;
    resolved_len_ += new_size;
    
    size_t curr_body_len = buffer_->Length() - first_line_len_ - header_len_;
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

int http::HttpParser::DoParse() {
    if (buffer_->Length() <= 0) {
        LogE("buffer_->len: %zd", buffer_->Length())
        return 0;
    }
    size_t unresolved_len = buffer_->Length() - resolved_len_;
    if (unresolved_len <= 0) {
        LogI("no bytes need to be resolved: %zd", unresolved_len)
        return -1;
    }
    
    while (true) {
        bool success = false;
        if (position_ == kNone) {
            if (resolved_len_ == 0 && buffer_->Length() > 0) {
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
            return 0;
        }
        if (position_ == kError) {
            LogI("error occurred, do nothing")
            return -1;
        }
        if (!success) {
            return -1;
        }
    }
}

void http::HttpParser::Reset() {
    position_ = TPosition::kNone;
    headers_->Reset();
    first_line_len_ = 0;
    header_len_ = 0;
    resolved_len_ = 0;
    buffer_->Reset();      // careful
}

