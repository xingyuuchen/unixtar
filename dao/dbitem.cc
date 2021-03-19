#include "dbitem.h"
#include "log.h"


std::string &DBItem::field_list() const {
    return const_cast<std::string &>(field_list_str_);
}

std::string &DBItem::value_list() const {
    return const_cast<std::string &>(value_list_str_);
}

std::string &DBItem::equal_list() const {
    return const_cast<std::string &>(equal_list_str_);
}

void DBItem::__Format(std::vector<std::string> &_in, std::string&_out) {
    if (_in.empty()) {
        LogE(__FILE__, "func: PopulateXXXList Not Implemented!")
        return;
    }
    for (auto &it : _in) {
        _out.append(it);
        _out.append(",");
    }
    _out.erase(_out.end() - 1);
}

void DBItem::OnDataPrepared() {
    // parse field list
    std::vector<std::string> field_list;
    PopulateFieldList(field_list);
    __Format(field_list, field_list_str_);
    LogI(__FILE__, "[OnDataPrepared] field_list: %s", field_list_str_.c_str())
    
    // parse value list
    std::vector<std::string> value_list;
    PopulateValueList(value_list);
    __Format(value_list, value_list_str_);
    LogI(__FILE__, "[OnDataPrepared] value_list: %s", value_list_str_.c_str())
    
    // parse equal list
    std::map<std::string, std::string> kv;
    PopulateEqualList(kv);
    if (!kv.empty()) {
        for (auto it = kv.begin(); it != kv.end(); ++it) {
            equal_list_str_.append(it->first);
            equal_list_str_.append("=");
            equal_list_str_.append(it->second);
            equal_list_str_.append(" and ");
        }
        equal_list_str_ = equal_list_str_.substr(0, equal_list_str_.length() - 5);
    }
    LogI(__FILE__, "[OnDataPrepared] equal_list: %s", equal_list_str_.c_str())
}
