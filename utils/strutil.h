#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include <string>


namespace str {

char *strnstr(const char *_haystack, const char *_needle, size_t _len);


void split(const std::string &_src, const std::string &_separator,
                  std::vector<std::string> &_res);


std::string &trim(std::string &_s);


std::string &delafter(std::string &_str, std::string &_whence);


std::string &delafter(std::string &_str, char _whence);


}

