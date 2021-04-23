#include "yamlutil.h"
#include "log.h"
#include "strutil.h"
#include <stdio.h>
#include <string>
#include <fstream>
#include <set>
#include <map>

namespace yaml {

static std::set<YamlDescriptor> sg_open_descs;
static std::map<YamlDescriptor, std::map<std::string, std::string>> sg_yaml_cache;

int _ParseYaml(const char *_path, YamlDescriptor _desc) {
    std::ifstream fin(_path);
    if (!fin) {
        LogE("!fin: %s", _path)
        return -1;
    }
    
    std::map<std::string, std::string> kv;
    std::string s;
    while (getline(fin, s)) {
        std::vector<std::string> line;
        if (s.empty() || s[0] == '#') {
            continue;
        }
        std::string::size_type find = s.find('#');
        if (find != std::string::npos) {
            s = s.substr(0, find);
        }
        str::split(s, ": ", line);
        if (line.size() != 2) {
            LogI("%s: incorrect yaml formula, line: %s",
                 _path, s.c_str())
            return -1;
        }
        kv.insert(std::make_pair(line[0], line[1]));
    }
    sg_yaml_cache.insert(std::make_pair(_desc, kv));
    return 0;
}

void _GetFromCache(YamlDescriptor _desc, std::string &_field, std::string &_res) {
    if (sg_yaml_cache.find(_desc) == sg_yaml_cache.end()) {
        LogI("(%p) not in sg_yaml_cache", _desc)
        return;
    }
    std::map<std::string, std::string> &kv = sg_yaml_cache[_desc];
    if (kv.find(_field) == kv.end()) {
        LogI("field(%s) not in sg_yaml_cache", _field.c_str())
        return;
    }
    _res = kv[_field];
}

YamlDescriptor Load(const char *_path) {
    FILE *fp = fopen(_path, "r");
    if (fp) {
        sg_open_descs.insert(fp);
        _ParseYaml(_path, fp);
        return fp;
    }
    LogE("fp == NULL")
    return NULL;
}

int Get(YamlDescriptor _desc, std::string &_field, int &_res) {
    if (IsOpen(_desc)) {
        std::string cache;
        _GetFromCache(_desc, _field, cache);
        if (cache.empty()) {
            return -1;
        }
        _res = (int) strtol(cache.c_str(), NULL, 10);
        return 0;
    }
    return -1;
}

int Get(YamlDescriptor _desc, std::string &_field, uint16_t &_res) {
    if (IsOpen(_desc)) {
        std::string cache;
        _GetFromCache(_desc, _field, cache);
        if (cache.empty()) {
            return -1;
        }
        _res = (uint16_t) strtol(cache.c_str(), NULL, 10);
        return 0;
    }
    return -1;
}

int Get(YamlDescriptor _desc, std::string &_field, std::string &_res) {
    if (IsOpen(_desc)) {
        std::string cache;
        _GetFromCache(_desc, _field, cache);
        if (cache.empty()) {
            return -1;
        }
        _res = std::move(cache);
        return 0;
    }
    return -1;
}

int Get(YamlDescriptor _desc, std::string &_field, double &_res) {
    if (IsOpen(_desc)) {
        std::string cache;
        _GetFromCache(_desc, _field, cache);
        if (cache.empty()) {
            return -1;
        }
        _res = strtod(cache.c_str(), NULL);
        return 0;
    }
    return -1;
}


int Close(YamlDescriptor _desc) {
    sg_open_descs.erase(_desc);
    sg_yaml_cache.erase(_desc);
    return fclose(_desc);
}

bool IsOpen(YamlDescriptor _desc) {
    return sg_open_descs.find(_desc) != sg_open_descs.end();
}

}
