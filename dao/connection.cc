#include "connection.h"
#include <mysql++.h>
#include <string>
#include "log.h"
#include "yamlutil.h"


namespace Dao {

static std::string kDatabase = "database";
static std::string kServer = "server";
static std::string kUser = "user";
static std::string kPassword = "password";

class _Connection {
    
    SINGLETON(_Connection, )
    
  public:
    enum TStatus {
        kNotConnected = 0,
        kConnected,
        kBusy,
    };
    
    ~_Connection() {
        LogI(__FILE__, "[_Connection] disconnect database.")
        conn_.disconnect();
    }
    
    bool IsConnected() { return status_ == kConnected; }
    
    void Config() {
        Yaml::YamlDescriptor desc = Yaml::Load("../framework/dao/database.yaml");
        if (desc) {
            Yaml::GetString(desc, kDatabase, db_);
            Yaml::GetString(desc, kServer, svr_);
            Yaml::GetString(desc, kUser, usr_);
            Yaml::GetString(desc, kPassword, pwd_);
            Yaml::Close(desc);
            LogI(__FILE__, "db: %s, svr: %s, usr: %s, pwd: %s", db_.c_str(),
                 svr_.c_str(), usr_.c_str(), pwd_.c_str())
            __TryConnect();
            return;
        }
        LogE(__FILE__, "[Config] desc == NULL")
    }
    
    int Insert(DBItem &_row) {
        if (!IsConnected()) {
            LogE(__FILE__, "[Insert] kNotConnected")
            return -1;
        }
        try {
            mysqlpp::Query query = conn_.query();
            query.insert(_row);
            LogI(__FILE__, "[Insert] %s", query.str().c_str())
            query.exec();
            return query.insert_id();
        } catch (const mysqlpp::BadQuery &er) {
            LogE(__FILE__, "[Insert] BadQuery: %s", er.what());
        } catch (const mysqlpp::BadConversion &er) {
            LogE(__FILE__, "[Insert] Conversion error: %s, retrieved data size: %ld, actual size: %ld",
                 er.what(), er.retrieved, er.actual_size);
        }
        return -1;
    }
    
    int Query(const char *_sql, std::vector<std::string> &_res, int _col_cnt) {
        if (!IsConnected()) {
            LogE(__FILE__, "[Query] kNotConnected")
            return -1;
        }
        try {
            mysqlpp::Query query = conn_.query(_sql);
//            query.storein(_res);
            if (mysqlpp::StoreQueryResult res = query.store()) {
                mysqlpp::StoreQueryResult::const_iterator it;
                for (it = res.begin(); it != res.end(); ++it) {
                    mysqlpp::Row row = *it;
                    for (int i = 0; i < _col_cnt; ++i) {
                        _res.emplace_back(row[i]);
                    }
                }
            }
            return 0;
        } catch (const mysqlpp::BadQuery &er) {
            LogE(__FILE__, "[Query] BadQuery: %s", er.what());
        } catch (const mysqlpp::BadConversion &er) {
            LogE(__FILE__, "[Query] Conversion error: %s, retrieved data size: %ld, actual size: %ld",
                 er.what(), er.retrieved, er.actual_size);
        } catch (const mysqlpp::Exception& er) {
            LogE(__FILE__, "[Query] BadQuery: %s", er.what());
        }
        return -1;
    }
    
    int Update(DBItem &_o, DBItem &_n) {
        if (!IsConnected()) {
            LogE(__FILE__, "[Update] kNotConnected")
            return -1;
        }
        try {
            mysqlpp::Query query = conn_.query();
            query.update(_o, _n);
            LogI(__FILE__, "[Insert] %s", query.str().c_str())
            query.exec();
            return 0;
        } catch (const mysqlpp::BadQuery &er) {
            LogE(__FILE__, "[Update] BadQuery: %s", er.what());
        } catch (const mysqlpp::BadConversion &er) {
            LogE(__FILE__, "[Update] Conversion error: %s, retrieved data size: %ld, actual size: %ld",
                 er.what(), er.retrieved, er.actual_size);
        }
        return -1;
    }
    
  private:
    int __TryConnect() {
        while (retry_cnt_--) {
            try {
                if (conn_.connect(db_.c_str(), svr_.empty() ? "localhost" : svr_.c_str(),
                                  usr_.c_str(), pwd_.c_str())) {
                    LogI(__FILE__, "[TryConnect] succeed.")
//                    conn_.set_option(new mysqlpp::SetCharsetNameOption(MYSQLPP_UTF8_CS));
                    mysqlpp::Query query = conn_.query("SET names 'utf8mb4'");
                    query.exec();
                    status_ = kConnected;
                    return 0;
                } else {
                    LogE(__FILE__, "[TryConnect] failed.")
                    status_ = kNotConnected;
                }
            } catch (const mysqlpp::Exception& er) {
                LogE(__FILE__, "[__TryConnect] BadQuery: %s", er.what());
                return -1;
            }
        }
        return -1;
    }
    
  private:
    mysqlpp::Connection     conn_;
    int                     retry_cnt_;
    std::string             db_;
    std::string             usr_;
    std::string             pwd_;
    std::string             svr_;
    TStatus                 status_;
};

_Connection::_Connection()
        : status_(kNotConnected)
        , conn_(false)
        , retry_cnt_(3) {
    Config();
}

int Insert(DBItem &_row) {
    _row.OnDataPrepared();
    return _Connection::Instance().Insert(_row);
}

int Update(DBItem &_o, DBItem &_n) {
    _o.OnDataPrepared();
    _n.OnDataPrepared();
    return _Connection::Instance().Update(_o, _n);
}

int Query(const char *_sql, std::vector<std::string> &_res, int _col_cnt) {
    return _Connection::Instance().Query(_sql, _res, _col_cnt);
}

bool IsConnected() { return _Connection::Instance().IsConnected(); }


}
