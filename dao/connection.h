#pragma once

#include "singleton.h"


namespace Dao {

class Connection {
    SINGLETON(Connection, )
    
  public:
    void Connect(const char *_db, const char *_pass, const char *_user="root");
  
};

}
