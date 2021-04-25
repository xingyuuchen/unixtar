#include "connection.h"
#include <mysql++.h>
#include <string>
#include "log.h"
#include "yamlutil.h"
#include "threadpool/threadpool.h"


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
        LogI("disconnect database.")
        conn_.disconnect();
    }
    
    bool IsConnected() { return status_ == kConnected; }
    
    void Config() {
        yaml::YamlDescriptor desc = yaml::Load("../framework/dao/dbconfig.yml");
        if (desc) {
            yaml::Get(desc, kDatabase, db_);
            yaml::Get(desc, kServer, svr_);
            yaml::Get(desc, kUser, usr_);
            yaml::Get(desc, kPassword, pwd_);
            yaml::Close(desc);
            LogI("db: %s, svr: %s, usr: %s, pwd: %s", db_.c_str(),
                 svr_.c_str(), usr_.c_str(), pwd_.c_str())
            __TryConnect();
    
            LogI("Register Mysql HeartBeat Task.")
            const int kHeartBeatPeriod = 4 * 60 * 1000;    // mysql default wait_timeout is 8 hours.
            ThreadPool::Instance().ExecutePeriodic(kHeartBeatPeriod, [this] {
                __SendHeartbeat();
            });
            return;
        }
        LogE("please config your mysql server, database, user, password in dbconfig.yaml")
    }
    
    int Insert(DBItem &_row) {
        if (!__CheckConnection()) {
            LogE("kNotConnected")
            return -1;
        }
        try {
            mysqlpp::Query query = conn_.query();
            query.insert(_row);
            LogI("%s", query.str().c_str())
            query.exec();
            return query.insert_id();
        } catch (const mysqlpp::BadQuery &er) {
            LogE("BadQuery: %s", er.what());
        } catch (const mysqlpp::BadConversion &er) {
            LogE("Conversion error: %s, retrieved data size: %ld, actual size: %ld",
                 er.what(), er.retrieved, er.actual_size);
        }
        return -1;
    }
    
    int Query(const char *_sql, std::vector<std::string> &_res, int _col_cnt) {
        if (!__CheckConnection()) {
            LogE("kNotConnected")
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
            LogE("BadQuery: %s", er.what());
        } catch (const mysqlpp::BadConversion &er) {
            LogE("Conversion error: %s, retrieved data size: %ld, actual size: %ld",
                 er.what(), er.retrieved, er.actual_size);
        } catch (const mysqlpp::Exception& er) {
            LogE("BadQuery: %s", er.what());
        }
        return -1;
    }
    
    int Update(DBItem &_o, DBItem &_n) {
        if (!__CheckConnection()) {
            LogE("kNotConnected")
            return -1;
        }
        try {
            mysqlpp::Query query = conn_.query();
            query.update(_o, _n);
            LogI("%s", query.str().c_str())
            query.exec();
            return 0;
        } catch (const mysqlpp::BadQuery &er) {
            LogE("BadQuery: %s", er.what());
        } catch (const mysqlpp::BadConversion &er) {
            LogE("Conversion error: %s, retrieved data size: %ld, actual size: %ld",
                 er.what(), er.retrieved, er.actual_size);
        }
        return -1;
    }
    
  private:
    bool __CheckConnection() {
        status_ = conn_.connected() ? kConnected : kNotConnected;
        if (status_ == kConnected) {
            return true;
        }
        return __TryConnect() == 0;
    }
    
    int __TryConnect() {
        int retry = 3;
        while (retry--) {
            try {
                if (conn_.connect(db_.c_str(), svr_.empty() ? "localhost" : svr_.c_str(),
                                  usr_.c_str(), pwd_.c_str())) {
                    LogI("succeed.")
//                    conn_.set_option(new mysqlpp::SetCharsetNameOption(MYSQLPP_UTF8_CS));
                    mysqlpp::Query query = conn_.query("SET names 'utf8mb4'");
                    query.exec();
                    status_ = kConnected;
                    return 0;
                } else {
                    LogE("failed.")
                    status_ = kNotConnected;
                }
            } catch (const mysqlpp::Exception& er) {
                LogE("BadQuery: %s", er.what());
                return -1;
            }
        }
        return -1;
    }
    
    void __SendHeartbeat() {
        if (!__CheckConnection()) {
            LogE("SendHeartBeat failed: connection already lost!!")
            return;
        }
        if (!conn_.ping()) {
            LogI("ping db server failed, server_status: %s", conn_.server_status().c_str())
        }
    }
    
  private:
    mysqlpp::Connection     conn_;
    std::string             db_;
    std::string             usr_;
    std::string             pwd_;
    std::string             svr_;
    TStatus                 status_;
};

_Connection::_Connection()
        : status_(kNotConnected)
        , conn_(false) {
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
