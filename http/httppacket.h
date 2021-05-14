#pragma once

#include "firstline.h"
#include "headerfield.h"
#include "autobuffer.h"
#include "networkmodel/applicationlayer.h"
#include <map>


namespace http {

class HttpParser;

class HttpPacket : public ApplicationPacket {
  public:
    HttpPacket();
    
    ~HttpPacket() override;
    
    TApplicationProtocol ApplicationProtocol() const override;
    
    virtual size_t ContentLength() const;
    
    http::HeaderField *Headers();
    
    void SetBody(char *_ptr, size_t _length);
    
    virtual AutoBuffer *Body();

  protected:
    http::HeaderField   headers_;
    AutoBuffer          body_;
  
  private:
  
};


class HttpParser : public ApplicationProtocolParser {
  public:
    enum TPosition {
        kNone = 0,
        kFirstLine,
        kHeaders,
        kBody,
        kEnd,
        kError,
    };
    
    HttpParser(http::HttpPacket *_http_packet,
               AutoBuffer *_buff);
    
    ~HttpParser() override;
    
    int DoParse() override;
    
    void Reset();
    
    bool IsEnd() const override;
    
    bool IsErr() const override;
    
    TPosition GetPosition() const;
    
  protected:
    virtual bool _ResolveFirstLine() = 0;
    
    bool _ResolveHeaders();
    
    virtual bool _ResolveBody();
    
  protected:
    TPosition                               position_;
    http::HttpPacket                      * http_packet_;
    http::HeaderField                     * headers_;
    size_t                                  first_line_len_;
    size_t                                  header_len_;
    size_t                                  resolved_len_;
    AutoBuffer                            * buffer_;
};

}

