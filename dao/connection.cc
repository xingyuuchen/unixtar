#include "connection.h"
//#include <mysql++.h>
#include "log.h"

namespace Dao {

Connection::Connection() {

}

void Connection::Connect(const char *_db, const char *_pass, const char *_user) {
    
//    mysqlpp::Connection conn(false);
//    if (conn.connect(_db, "localhost", _user, _pass)) {
//        mysqlpp::Query query = conn.query("select * from users");
//        if (mysqlpp::StoreQueryResult res = query.store()) {
//            mysqlpp::StoreQueryResult::const_iterator it;
//            for (it = res.begin(); it != res.end(); ++it) {
//                mysqlpp::Row row = *it;
//                LogI(__FILE__, "%s-%s-%s-%s", row[0].c_str(), row[1].c_str(), row[2].c_str(), row[3].c_str())
//            }
//        }
//    } else {
        LogE(__FILE__, "connect db fail.")
//    }
    
}

}