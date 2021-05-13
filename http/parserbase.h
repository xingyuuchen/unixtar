#pragma once

#include "firstline.h"
#include "headerfield.h"
#include "autobuffer.h"
#include <map>

namespace http {

class ParserBase {
  public:
    enum TPosition {
        kNone = 0,
        kFirstLine,
        kHeaders,
        kBody,
        kEnd,
        kError,
    };
    
    ParserBase();
    
    virtual ~ParserBase();
    
    void DoParse();
    
    void Reset();
    
    bool IsEnd() const;
    
    bool IsErr() const;
    
    http::HeaderField &Headers();
    
    virtual bool IsHttpRequest() const = 0;
    
    AutoBuffer *GetBuff();
    
    virtual char *GetBody();
    
    TPosition GetPosition() const;
    
    virtual size_t GetContentLength() const;

  protected:
    virtual bool _ResolveFirstLine() = 0;
    
    bool _ResolveHeaders();
    
    virtual bool _ResolveBody();
    
  protected:
    TPosition                               position_;
    http::HeaderField                       headers_;
    size_t                                  first_line_len_;
    size_t                                  header_len_;
    size_t                                  resolved_len_;
    AutoBuffer                              buff_;
};

}

