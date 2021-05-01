#pragma once

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "autobuffer.h"


#define SOCKET int
#define INVALID_SOCKET (-1)
#define IS_EAGAIN(errno) ((errno) == EAGAIN || (errno) == EWOULDBLOCK)
#define CLOSE_SOCKET ::close


class Socket {
  public:
    
    explicit Socket(SOCKET _fd, int _type = SOCK_STREAM, bool _nonblocking = true);
    
    int Create(int _domain, int _type, int _protocol);
    
    ssize_t Recv(AutoBuffer *_buff, bool *_is_buffer_full);
    
    bool IsEAgain() const;
    
    void Set(SOCKET _fd);
    
    void Close();
    
    bool IsTcp() const;
    
    bool IsUdp() const;
    
    SOCKET FD() const;
    
    ~Socket();
    
    int SetNonblocking();
    
    int SetSocketOpt(int _level, int _option_name,
                     const void *_option_value,
                     socklen_t _option_len) const;
    
    bool IsNonblocking() const;

  private:
    static const int    kBufferSize;
    SOCKET              fd_;
    int                 errno_;
    bool                is_eagain_;
    int                 type_;
    bool                nonblocking_;
};

