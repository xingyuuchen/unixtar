#include "parsermanager.h"
#include "log.h"


namespace http { namespace request {


ParserManager::ParserManager() { }

std::shared_ptr<http::request::Parser> ParserManager::GetParser(SOCKET _fd) {
    auto find = parsers_map_.find(_fd);
    if (find != parsers_map_.end()) {
        return find->second;
    }
    return __CreateParser(_fd);
}

std::shared_ptr<http::request::Parser> ParserManager::__CreateParser(SOCKET _fd) {
    auto new_parser = std::make_shared<http::request::Parser>();
    if (!parsers_map_.emplace(_fd, new_parser).second) {
        LogE(__FILE__, "[__CreateParser] insert failed")
    }
    LogI(__FILE__, "[__CreateParser] map size: %lu", parsers_map_.size())
    return new_parser;
}

int ParserManager::DelParser(SOCKET _fd) {
    int ret = parsers_map_.erase(_fd);
    LogI(__FILE__, "[DelParser] map size: %lu, ret: %d", parsers_map_.size(), ret)
    return ret;
}

int ParserManager::ContainsParser(SOCKET _fd) {
    return parsers_map_.find(_fd) != parsers_map_.end();
}

}}