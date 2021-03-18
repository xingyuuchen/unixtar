#pragma once
#include <inttypes.h>
#include <stdlib.h>
#include <string>


namespace Yaml {

using YamlDescriptor = FILE *;

YamlDescriptor Load(const char *_path);

bool IsOpen(YamlDescriptor _desc);

void _GetFromCache(YamlDescriptor _desc, std::string &_filed, std::string &_res);

template<class T>
int Get(YamlDescriptor _desc, std::string &_filed, T &_res) {
    if (IsOpen(_desc)) {
        std::string cache;
        _GetFromCache(_desc, _filed, cache);
        if (cache.empty()) {
            return -1;
        }
        if (std::is_same<T, int>::value || std::is_same<T, long>::value) {
            _res = strtol(cache.c_str(), NULL, 10);
            return 0;
        } else if (std::is_same<T, float>::value || std::is_same<T, double>::value) {
            _res = strtod(cache.c_str(), NULL);
        } else if (std::is_same<T, std::string>::value) {
            _res = std::move(cache);
            return 0;
        }
    }
    return -1;
}

int Close(YamlDescriptor _desc);

}
