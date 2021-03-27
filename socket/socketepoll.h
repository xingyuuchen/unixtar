#ifndef OI_SVR_SOCKETEPOLL_H
#define OI_SVR_SOCKETEPOLL_H

#ifdef __linux__
#include <sys/epoll.h>
#else
#ifdef __APPLE__
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#define epoll_event kevent
#define epoll_data_t void
#endif
#endif
#include <stddef.h>
#include "unix_socket.h"


class SocketEpoll {
  public:
    
    explicit SocketEpoll(int _max_fds = 1024);
    
    ~SocketEpoll();
    
    void SetListenFd(SOCKET _listen_fd);
    
    int EpollWait(int _timeout_mills = -1, int _max_events = kMaxFds);
    
    int AddSocketRead(SOCKET _fd);
    
    int ModSocketWrite(SOCKET _fd, void *_ptr);
    
    int DelSocket(SOCKET _fd);
    
    SOCKET GetSocket(int _idx);
    
    int IsReadSet(int _idx);
    
    void *IsWriteSet(int _idx);
    
    int IsErrSet(int _idx);
    
    bool IsNewConnect(int _idx);
    
    int GetErrNo() const;
    
    
  private:
    
    /**
     * @param _idx: varies from 0 to the val as specifies by
     *              return val of EpollWait().
     * @param _flag: EPOLLIN, EPOLLOUT, EPOLLERR, etc.
     * @return: NULL if flag not set, else epoll_event.data.
     */
    epoll_data_t *__IsFlagSet(int _idx, int _flag);
    
    int __EpollCtl(int _op, SOCKET _fd, struct epoll_event *_event = NULL);
    
  private:
    int                         epoll_fd_;
    int                         listen_fd_;
    struct epoll_event*         epoll_events_;
    int                         errno_;
    static const int            kMaxFds;
    
};


#endif //OI_SVR_SOCKETEPOLL_H
