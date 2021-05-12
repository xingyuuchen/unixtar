#pragma once
#include "netscenecustom.h"
#include "dbitem.h"
#include "dao/connection.h"
#include <mutex>


class NetSceneGetIndexPage : public NetSceneCustom {
  public:
    
    NetSceneGetIndexPage();
    
    int GetType() override;
    
    NetSceneBase *NewInstance() override;
    
    int DoSceneImpl(const std::string &_in_buffer) override;
    
    void *Data() override;
    
    size_t Length() override;
    
    const char *Route() override;
    
    const char *ContentType() override;

private:
    char                        resp_[128] {0, };
    static uint64_t             visit_times_start;
    static uint64_t             visit_times_this_boot;
    static const char *const    kUrlRoute;
    static const char *const    kRespFormat;
    static std::mutex           mutex_;
    
};


class DBItemVisit : public DBItem {
  public:
    DBItemVisit();
    
    ~DBItemVisit();
    
    const char *table() const override;
    
    void PopulateFieldList(std::vector<std::string> &_field_list) const override;
    
    void PopulateValueList(std::vector<std::string> &_value_list) const override;
    
    void PopulateEqualList(std::map<std::string, std::string> &_equal_list) const override;

    void SetVisitTimes(uint64_t _times);
    
    uint64_t GetVisitTimes() const;

  public:
    static const char *const    kTable;
    static const char *const    kFieldNameVisitTimes;
  private:
    uint64_t                    visit_times;
    bool                        has_visit_times;
};
