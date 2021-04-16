#ifndef OI_SVR_STRUTIL_H
#define OI_SVR_STRUTIL_H

#include <stdint.h>
#include <string.h>
#include <vector>
#include <string>


namespace str {

inline char *strnstr(const char *_haystack,
                     const char *_needle, size_t _len) {
    if (_haystack == NULL || _needle == NULL) { return NULL; }
    
    int len1, len2;
    len2 = (int) strlen(_needle);
    
    if (!len2) { return (char *) _haystack; }
    
    len1 = (int) strnlen(_haystack, _len);
    _len = _len > len1 ? len1 : _len;
    
    while (_len >= len2) {
        _len--;
        if (!memcmp(_haystack, _needle, (size_t) len2)) {
            return (char *) _haystack;
        }
        _haystack++;
    }
    
    return NULL;
}

inline void split(const std::string &_src, const std::string &_separator,
                  std::vector<std::string> &_res) {
    _res.clear();
    if (_src.empty()) { return; }
    size_t size = _separator.size();
    
    std::string::size_type curr, last = 0;
    bool has = false;
    while ((curr = _src.find(_separator, last)) != std::string::npos) {
        if (curr == last) {
            last = curr + size;
            continue;
        }
        has = true;
        _res.push_back(_src.substr(last, curr - last));
        last = curr + size;
    }
    if (has && last < _src.size()) {
        _res.push_back(_src.substr(last, _src.size() - last));
    }
}

}

#endif //OI_SVR_STRUTIL_H
