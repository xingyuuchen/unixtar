#include "netscene_getindexpage.h"
#include "constantsprotocol.h"
#include "log.h"
#include "http/headerfield.h"
#include "signalhandler.h"
#include "fileutil.h"
#include <cassert>


const char *const NetSceneGetIndexPage::kUrlRoute = "/";
uint64_t NetSceneGetIndexPage::visit_times_start = 0;
uint64_t NetSceneGetIndexPage::visit_times_this_boot = 0;

const char *const NetSceneGetIndexPage::kRespFormat =
        "If you see this, Unixtar is running normally, %llu historical visits.";

std::mutex NetSceneGetIndexPage::mutex_;


NetSceneGetIndexPage::NetSceneGetIndexPage()
        : NetSceneCustom() {
    NETSCENE_INIT_START
        std::vector<std::string> visit_times;
        char sql[256] = {0, };
        snprintf(sql, sizeof(sql), "select %s from %s;",
                 DBItemVisit::kFieldNameVisitTimes,
                 DBItemVisit::kTable);
        int db_ret = dao::Query(sql, visit_times);
        if (db_ret < 0) {
            LogE("query visit times failed, db err")
            return;
        }
        if (visit_times.size() != 1) {
            LogE("query visit times failed, size: %lu", visit_times.size())
            return;
        }
        
        visit_times_start = std::stoull(visit_times[0], nullptr);
        LogI("visit_times_start: %llu", visit_times_start)
        visit_times_this_boot = visit_times_start;
        
        SignalHandler::Instance().RegisterCallback(SIGINT, [=] {
            DBItemVisit old;
            old.SetVisitTimes(visit_times_start);
            DBItemVisit neo;
            neo.SetVisitTimes(visit_times_this_boot);
            dao::Update(old, neo);
        });
    NETSCENE_INIT_END
}

int NetSceneGetIndexPage::GetType() { return kNetSceneTypeGetIndexPage; }

NetSceneBase *NetSceneGetIndexPage::NewInstance() { return new NetSceneGetIndexPage(); }

int NetSceneGetIndexPage::DoSceneImpl(const std::string &_in_buffer) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    snprintf(resp_, sizeof(resp_), kRespFormat, ++visit_times_this_boot);
    
    return 0;
}

const char *NetSceneGetIndexPage::ContentType() {
    return http::HeaderField::kTextPlain;
}

void *NetSceneGetIndexPage::Data() {
    return resp_;
}

size_t NetSceneGetIndexPage::Length() {
    LogI("%lu", strlen(resp_))
    return strlen(resp_);
}

const char *NetSceneGetIndexPage::Route() {
    return kUrlRoute;
}



const char *const DBItemVisit::kTable = "visit_times";
const char *const DBItemVisit::kFieldNameVisitTimes = "visit_times";

DBItemVisit::DBItemVisit()
        : DBItem()
        , visit_times(0)
        , has_visit_times(false) {
}

const char *DBItemVisit::table() const { return kTable; }

void DBItemVisit::PopulateFieldList(std::vector<std::string> &_field_list) const {
    if (has_visit_times) {
        _field_list.emplace_back(kFieldNameVisitTimes);
    }
}

void DBItemVisit::PopulateValueList(std::vector<std::string> &_value_list) const {
    if (has_visit_times) {
        _value_list.emplace_back(std::to_string(visit_times));
    }
}

void DBItemVisit::PopulateEqualList(std::map<std::string, std::string> &_equal_list) const {
    if (has_visit_times) {
        _equal_list.insert(std::make_pair(kFieldNameVisitTimes,
                          std::to_string(visit_times)));
    }
}

void DBItemVisit::SetVisitTimes(uint64_t _times) {
    has_visit_times = true;
    visit_times = _times;
}

uint64_t DBItemVisit::GetVisitTimes() const { return visit_times; }

DBItemVisit::~DBItemVisit() = default;



const char *const NetSceneGetFavIcon::kUrlRoute = "/favicon.ico";
std::string NetSceneGetFavIcon::kFavIconData;

NetSceneGetFavIcon::NetSceneGetFavIcon()
        : NetSceneCustom() {
    NETSCENE_INIT_START
        std::string curr(__FILE__);
        std::string path404 = curr.substr(0, curr.rfind('/')) + "/res/favicon.png";
        
        if (!file::ReadFile(path404.c_str(), kFavIconData)) {
            LogE("read favicon.jpeg failed")
            assert(false);
        }
    NETSCENE_INIT_END
}

int NetSceneGetFavIcon::GetType() { return kNetSceneTypeGetFavIcon; }

NetSceneBase *NetSceneGetFavIcon::NewInstance() { return new NetSceneGetFavIcon(); }

int NetSceneGetFavIcon::DoSceneImpl(const std::string &_in_buffer) {
    // nothing to do.
    return 0;
}

void *NetSceneGetFavIcon::Data() { return (void *) kFavIconData.data(); }

size_t NetSceneGetFavIcon::Length() { return kFavIconData.size(); }

const char *NetSceneGetFavIcon::Route() { return kUrlRoute; }

const char *NetSceneGetFavIcon::ContentType() { return http::HeaderField::kImagePng; }

