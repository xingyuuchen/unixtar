#pragma once
#include <cstring>
#include <cstdint>
#include "exception.h"


namespace base64 {

class IllegalBase64LengthException : public unixtar::Exception {
  public:
    explicit IllegalBase64LengthException(size_t _len);
};

class Base64DecodeException : public unixtar::Exception {
  public:
    Base64DecodeException();
};

size_t Encode(const unsigned char *_in, size_t _in_len, char *_res);

size_t Decode(const char *_in, size_t _in_len, unsigned char *_res);

void UnitTest();

}

