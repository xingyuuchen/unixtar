#include "exception.h"

namespace unixtar {

Exception::Exception(std::string &&_msg)
        : msg_(std::move(_msg)) {
}

Exception::Exception(char *_msg)
        : msg_(_msg) {
}

Exception::Exception(const Exception &_another) noexcept {
    msg_ = _another.msg_;
}

Exception &Exception::operator=(const Exception &) noexcept {
    return *this;
}

const char *Exception::what() const noexcept {
    return msg_.c_str();
}

Exception::~Exception() = default;

}