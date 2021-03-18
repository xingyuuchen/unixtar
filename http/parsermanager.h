#ifndef OI_SVR_PARSERMANAGER_H
#define OI_SVR_PARSERMANAGER_H

#include <unordered_map>
#include <memory>
#include "httprequest.h"
#include "unix_socket.h"
#include "singleton.h"


namespace http { namespace request {

class ParserManager {
    
    SINGLETON(ParserManager, )
    
  public:
    
    int DelParser(SOCKET _fd);
    
    int ContainsParser(SOCKET _fd);
    
    std::shared_ptr<http::request::Parser> GetParser(SOCKET _fd);
    
  private:
    
    std::shared_ptr<http::request::Parser> __CreateParser(SOCKET _fd);

  private:
    std::unordered_map<SOCKET, std::shared_ptr<http::request::Parser>> parsers_map_;
    
};

}}



#endif //OI_SVR_PARSERMANAGER_H
