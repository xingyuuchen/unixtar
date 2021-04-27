#ifndef OI_SVR_WEBSERVER_H
#define OI_SVR_WEBSERVER_H
#include <stdint.h>
#include <map>
#include <list>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <cassert>
#include "netscenebase.h"
#include "singleton.h"
#include "thread.h"
#include "http/connectionprofile.h"
#include "socket/socketepoll.h"
#include "messagequeue.h"


class WebServer final {
    
    SINGLETON(WebServer, )
    
    class NetThread;

  public:
    
    void Serve();
    
    void NotifyStop();
    
    ~WebServer();
    
    class ServerConfig {
      public:
        static std::string      field_port;
        static uint16_t         port;
        static std::string      field_net_thread_cnt;
        static size_t           net_thread_cnt;
        static std::string      field_max_backlog;
        static size_t           max_backlog;
        static bool             is_config_done;
    };

    class WorkerThread : public Thread {
      public:
        WorkerThread();
        
        void Run() final;
    
        virtual void HandleImpl(http::RecvContext *) = 0;
        
        virtual void HandleOverload(http::RecvContext *) = 0;
    
        void BindNetThread(NetThread *_net_thread);
    
        void NotifyStop();
    
        int GetWorkerSeqNum() const;
        
        ~WorkerThread() override;

      private:
        static int __MakeWorkerThreadSeq();
        
      private:
        NetThread *             net_thread_;
        const int               thread_seq_;
    };
    
    
    /**
     *
     * Sets the Worker-class who handles the business logic of all request,
     * by default one WorkerThread corresponds to one NetThread.
     *
     * The workers' life cycle is managed by framework,
     * just implement WorkerThread's {@func HandleImpl} and register it here.
     *
     */
    template<class WorkerImpl/* : public WorkerThread*/, class ...Args>
    void SetWorker(Args &&..._init_args) {
        
        assert(ServerConfig::is_config_done && ServerConfig::net_thread_cnt > 0);
        
        for (int i = 0; i < ServerConfig::net_thread_cnt; ++i) {
            // Initially, one network thread is bound to one worker thread.
            auto worker = new WorkerImpl(_init_args...);
            auto net_thread = net_threads_[i];
            net_thread->BindNewWorker(worker);
        }
    }
    
    using RecvQueue = MessageQueue::ThreadSafeDeque<http::RecvContext *>;
    using SendQueue = MessageQueue::ThreadSafeDeque<tcp::SendContext *>;
    
  private:
    
    class ConnectionManager final {
      public:
        ConnectionManager();
        
        ~ConnectionManager();
        
        void SetEpoll(SocketEpoll *_epoll);
        
        tcp::ConnectionProfile *GetConnection(SOCKET);
        
        void AddConnection(SOCKET _fd, std::string &_ip, uint16_t _port);
        
        void DelConnection(SOCKET);
        
        void ClearTimeout();
        
      private:
        using ScopedLock = std::lock_guard<std::mutex>;
        std::unordered_map<SOCKET, tcp::ConnectionProfile *> connections_;
        SocketEpoll *                                        socket_epoll_;
        std::mutex                                           mutex_;
    };
    
    
    class NetThread : public Thread {
      public:
        
        NetThread();
        
        ~NetThread() override;
        
        void Run() override;
    
        bool IsWorkerOverload();
        
        bool IsWorkerFullyLoad();
    
        size_t GetMaxBacklog() const;
    
        /**
         * @param _backlog: The maximum backlog for waiting queue (which holds parsed HTTP packets
                            but have not yet been processed by the WorkerThread).
         */
        void SetMaxBacklog(size_t _backlog);
    
        void NotifySend();
        
        void NotifyStop();
    
        void AddConnection(SOCKET _fd, std::string &_ip, uint16_t _port);
    
        void DelConnection(SOCKET);
    
        void HandleSend();
    
        void ClearTimeout();
    
        RecvQueue *GetRecvQueue();
    
        SendQueue *GetSendQueue();
        
        void BindNewWorker(WorkerThread *);
        
        void OnStart() override;
    
        void HandleException(std::exception &ex) override;

      private:
        void __NotifyWorkersStop();
        
        bool __IsNotifySend(EpollNotifier::Notification &) const;
    
        bool __IsNotifyStop(EpollNotifier::Notification &) const;
    
        int __OnReadEvent(SOCKET);
    
        int __OnWriteEvent(tcp::SendContext *);
    
        int __OnErrEvent(SOCKET);
        
        int __OnReadEventTest(SOCKET);
        
      private:
        SocketEpoll                         socket_epoll_;
        ConnectionManager                   connection_manager_;
        EpollNotifier                       epoll_notifier_;
        EpollNotifier::Notification         notification_send_;
        EpollNotifier::Notification         notification_stop_;
        RecvQueue                           recv_queue_;
        size_t                              max_backlog_;
        static const size_t                 kDefaultMaxBacklog;
        SendQueue                           send_queue_;
        std::list<WorkerThread *>           workers_;
        
        friend class WebServer;
    };
    
    void __NotifyNetThreadsStop();
    
    bool __IsNotifyStop(EpollNotifier::Notification &) const;
    
    int __OnConnect();
    
    void __AddConnection(SOCKET _fd, std::string &_ip, uint16_t _port);
    
    int __CreateListenFd();
    
    int __Bind(uint16_t _port) const;
    
    int __OnEpollErr(SOCKET);
    
  private:
    std::vector<NetThread *>            net_threads_;
    SocketEpoll                         socket_epoll_;
    EpollNotifier                       epoll_notifier_;
    EpollNotifier::Notification         notification_stop_;
    bool                                running_;
    SOCKET                              listenfd_;
    
};


#endif //OI_SVR_WEBSERVER_H
