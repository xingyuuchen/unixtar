#pragma once

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include "autobuffer.h"
#include <string>


#define SOCKET int
#define INVALID_SOCKET (-1)
#define IS_EAGAIN(errno) ((errno) == EAGAIN || (errno) == EWOULDBLOCK)


class Socket {
  public:
    
    explicit Socket(SOCKET _fd, int _type = SOCK_STREAM,
                    bool _nonblocking = true, bool _connected = false);
    
    int Create(int _domain, int _type, int _protocol = 0);
    
    int Bind(sa_family_t _sin_family, uint16_t _port,
             in_addr_t _in_addr = INADDR_ANY) const;
    
    int Connect(std::string &_ip, uint16_t _port);
    
    void SetConnected(bool _connected);
    
    int Listen(int _backlog) const;
    
    ssize_t Receive(AutoBuffer *_buff, bool *_is_buffer_full);
    
    ssize_t Send(AutoBuffer *_buff, bool *_is_send_done);
    
    bool IsEAgain() const;
    
    void Set(SOCKET _fd);
    
    int ShutDown(int _how) const;
    
    void Close();
    
    bool IsTcp() const;
    
    bool IsUdp() const;
    
    SOCKET FD() const;
    
    ~Socket();
    
    int SetNonblocking();
    
    int SetTcpNoDelay() const;
    
    int SetTcpKeepAlive() const;
    
    int SetCloseLingerTimeout(int _linger) const;
    
    int SetSocketOpt(int _level, int _option_name,
                     const void *_option_value,
                     socklen_t _option_len) const;
    
    bool IsNonblocking() const;
    
    int SocketError() const;

  private:
    static const int    kBufferSize;
    SOCKET              fd_;
    int                 errno_;
    bool                is_eagain_;
    bool                is_connected_;
    int                 type_;
    bool                nonblocking_;
};

