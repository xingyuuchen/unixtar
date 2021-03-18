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
  public:
    enum TStatus {
        kOK = 0,
        kNotConnected,
    };
    
    _Connection() : status_(kNotConnected), conn_(false), retry_cnt_(3) {
        Config();
    }
    
    int TryConnect() {
        while (retry_cnt_--) {
            if (conn_.connect(db_.c_str(), svr_.empty() ? "localhost" : svr_.c_str(),
                              usr_.c_str(), pwd_.c_str())) {
                LogI(__FILE__, "[TryConnect] succeed.")
                status_ = kOK;
                return 0;
            } else {
                LogE(__FILE__, "[TryConnect] failed.")
                status_ = kNotConnected;
            }
        }
        return -1;
    }
    
    int Insert(const char *_sql) {
        if (!IsConnected()) {
            LogE(__FILE__, "[Insert] kNotConnected")
            return -1;
        }
        
        return 0;
    }
    
    bool IsConnected() { return status_ == kOK; }
    
    void Config() {
        Yaml::YamlDescriptor desc = Yaml::Load("../framework/dao/database.yaml");
        if (desc) {
            Yaml::Get(desc, kDatabase, db_);
            Yaml::Get(desc, kServer, svr_);
            Yaml::Get(desc, kUser, usr_);
            Yaml::Get(desc, kPassword, pwd_);
            Yaml::Close(desc);
            LogI(__FILE__, "db: %s, svr: %s, usr: %s, pwd: %s", db_.c_str(),
                 svr_.c_str(), usr_.c_str(), pwd_.c_str())
            TryConnect();
            return;
        }
        LogE(__FILE__, "[Config] desc == NULL")
    }
    
    int Query(const char *_sql, std::vector<std::string> &_res) {
        if (!IsConnected()) {
            LogE(__FILE__, "[Query] kNotConnected")
            return -1;
        }
        mysqlpp::Query query = conn_.query(_sql);
        if (mysqlpp::StoreQueryResult res = query.store()) {
            mysqlpp::StoreQueryResult::const_iterator it;
            for (it = res.begin(); it != res.end(); ++it) {
                mysqlpp::Row row = *it;
                
                LogI(__FILE__, "%s-%s-%s-%s", row[0].c_str(), row[1].c_str(),
                     row[2].c_str(), row[3].c_str())
            }
        }
        return 0;
    }
    
    int Update(const char *_sql) {
        if (!IsConnected()) {
            LogE(__FILE__, "[Query] kNotConnected")
            return -1;
        }
    
        return 0;
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

static _Connection sg_conn;

int Insert(const char *_sql) { return sg_conn.Insert(_sql); }

int Update(const char *_sql) { return sg_conn.Insert(_sql); }

int Query(const char *_sql, std::vector<std::string> &_res) { return sg_conn.Query(_sql, _res); }

bool IsConnected() { return sg_conn.IsConnected(); }

void Config() { sg_conn.Config(); }


}
