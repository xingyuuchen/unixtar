#include "dbitem.h"
#include "log.h"


std::string &DBItem::field_list() const {
    return const_cast<std::string &>(field_list_str_);
}

std::string &DBItem::value_list() const {
    return const_cast<std::string &>(value_list_str_);
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
    std::vector<std::string> field_list;
    PopulateFieldList(field_list);
    __Format(field_list, field_list_str_);
    LogI(__FILE__, "[OnDataPrepared] field_list: %s", field_list_str_.c_str())
    
    std::vector<std::string> value_list;
    PopulateValueList(value_list);
    __Format(value_list, value_list_str_);
    LogI(__FILE__, "[OnDataPrepared] value_list: %s", value_list_str_.c_str())
}
