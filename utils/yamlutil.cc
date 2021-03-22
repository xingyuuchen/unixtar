#include "yamlutil.h"
#include "log.h"
#include "strutil.h"
#include <stdio.h>
#include <string>
#include <fstream>
#include <set>
#include <map>

namespace Yaml {

static std::set<YamlDescriptor> sg_open_descs;
static std::map<YamlDescriptor, std::map<std::string, std::string>> sg_yaml_cache;

int _ParseYaml(const char *_path, YamlDescriptor _desc) {
    std::ifstream fin(_path);
    if (!fin) {
        LogE(__FILE__, "[_ParseYaml] !fin: %s", _path)
        return -1;
    }
    
    std::map<std::string, std::string> kv;
    std::string s;
    while (getline(fin, s)) {
        std::vector<std::string> line;
        if (s.empty() || s[0] == '#') {
            continue;
        }
        oi::split(s, ": ", line);
        if (line.size() != 2) {
            LogI(__FILE__, "[_ParseYaml] %s: incorrect yaml formula, line: %s",
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
        LogI(__FILE__, "[_GetFromCache] (%p) not in sg_yaml_cache", _desc)
        return;
    }
    std::map<std::string, std::string> &kv = sg_yaml_cache[_desc];
    if (kv.find(_field) == kv.end()) {
        LogI(__FILE__, "[_GetFromCache] field(%s) not in sg_yaml_cache", _field.c_str())
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
    LogE(__FILE__, "[Load] fp == NULL")
    return NULL;
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
