#ifndef OI_SVR_SOCKETEPOLL_H
#define OI_SVR_SOCKETEPOLL_H

#ifdef __linux__
#include <sys/epoll.h>
#else
#ifdef __APPLE__
#include <sys/types.h>
#include <sys/event.h>
#include <ctime>
#define epoll_event kevent
#define epoll_data void
#endif
#endif
#include <cstddef>
#include "unixsocket.h"


class SocketEpoll {
  public:
    
    explicit SocketEpoll(int _max_fds = 1024);
    
    ~SocketEpoll();
    
    void SetListenFd(SOCKET _listen_fd);
    
    int EpollWait(int _timeout_mills = -1, int _max_events = kMaxFds);
    
    int AddSocketRead(SOCKET _fd);
    
    int AddSocketRead(SOCKET _fd, uint64_t _data);
    
    int AddSocketReadWrite(SOCKET _fd, uint64_t _data);
    
    int ModSocketWrite(SOCKET _fd, uint64_t _data);
    
    int DelSocket(SOCKET _fd);
    
    SOCKET GetSocket(int _idx);
    
    /**
     * @return: epoll_data.ptr
     */
    void *GetEpollDataPtr(int _idx);
    
    /**
     * @return: 0 if EPOLLIN not set,
     *          else epoll_data_t.u64
     */
    uint64_t IsReadSet(int _idx);
    
    /**
     * @return: 0 if EPOLLOUT not set,
     *          else epoll_data_t.u64
     */
    uint64_t IsWriteSet(int _idx);
    
    /**
     * @return: 0 if EPOLLERR not set,
     *          else epoll_data_t.u64
     */
    uint64_t IsErrSet(int _idx);
    
    bool IsNewConnect(int _idx);
    
    int GetErrNo() const;
    
    
  private:
    
    /**
     * @param _idx: varies from 0 to the val as specifies by
     *              return val of EpollWait().
     * @param _flag: EPOLLIN, EPOLLOUT, EPOLLERR, etc.
     * @return: NULL if flag not set, else &epoll_event.data.
     */
    epoll_data *__IsFlagSet(int _idx, int _flag);
    
    int __EpollCtl(int _op, SOCKET _fd, struct epoll_event *_event = nullptr);
    
  private:
    int                         epoll_fd_;
    int                         listen_fd_;
    struct epoll_event *        epoll_events_;
    int                         errno_;
    static const int            kMaxFds;
    
};


/**
 * Actively notify the epoll from waiting.
 */
class EpollNotifier {
  public:
    
    class Notification {
        friend class EpollNotifier;
        
      public:
        /**
         * NotifyId is merely a flag for you to tell
         * which notification it is, and the address it points to
         * does not signify anything else.
         *
         * See {@code Notification()}
         */
        using NotifyId = void const *;
    
        Notification();
        
        explicit Notification(NotifyId _notify_id);
        
        bool operator==(const Notification &another);
        
      private:
        NotifyId notify_id_;
    };
    
    EpollNotifier();
    
    void SetSocketEpoll(SocketEpoll *_epoll);
    
    void NotifyEpoll(Notification &_notification);
    
    SOCKET GetNotifyFd() const;
    
    ~EpollNotifier();

  private:
    Socket          socket_;
    SocketEpoll *   socket_epoll_;
};


#endif //OI_SVR_SOCKETEPOLL_H
