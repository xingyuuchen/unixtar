#pragma once

#include "singleton.h"
#include <vector>
#include <string>

namespace Dao {

int Insert(const char *_sql);

int Query(const char *_sql, std::vector<std::string> &_res);

void Config();

bool IsConnected();


}
