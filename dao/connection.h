#pragma once

#include "singleton.h"
#include <vector>
#include <string>
#include "dbitem.h"

namespace dao {

int Insert(DBItem &_row);

int Update(DBItem &_o, DBItem &_n);

// TODO optimize
int Query(const char *_sql, std::vector<std::string> &_res, int _col_cnt);

int QueryExist(DBItem &_row, bool &_exist);

bool IsConnected();


}
