#pragma once
#include <inttypes.h>
#include <stdlib.h>
#include <string>


namespace Yaml {

using YamlDescriptor = FILE *;

YamlDescriptor Load(const char *_path);

bool IsOpen(YamlDescriptor _desc);

int Get(YamlDescriptor _desc, std::string &_field, int &_res);

int Get(YamlDescriptor _desc, std::string &_field, uint16_t &_res);

int Get(YamlDescriptor _desc, std::string &_field, double &_res);

int Get(YamlDescriptor _desc, std::string &_field, std::string &_res);

int Close(YamlDescriptor _desc);

}
