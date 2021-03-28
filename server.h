#ifndef OI_SVR_SERVER_H
#define OI_SVR_SERVER_H
#include <stdint.h>
#include <map>
#include <list>
#include <mutex>
#include "netscenebase.h"
#include "singleton.h"
#include "thread.h"
#include "connectionprofile.h"
#include "socket/socketepoll.h"
#include "messagequeue.h"


class Server {
    
    SINGLETON(Server, )
    
    class NetThread;

  public:
    
    void Serve();
    
    void NotifyStop();
    
    ~Server();
    
    class ServerConfig {
      public:
        static std::string      field_port;
        static uint16_t         port;
        static std::string      field_net_thread_cnt;
        static size_t           net_thread_cnt;
        static bool             is_config_done;
    };

    class WorkerThread : public Thread {
      public:
        WorkerThread();
        
        void Run() final;
    
        virtual void HandleImpl(Tcp::RecvContext *_recv_ctx) = 0;
    
        void BindNetThread(NetThread *_net_thread);
    
        void NotifyStop();
    
        /**
         * @return: whether @param _recv_ctx is the notification
         * from another thread to terminate this worker thread.
         */
        bool IsNotifyExit(Tcp::RecvContext *_recv_ctx);
        
        ~WorkerThread() override;

      protected:
        NetThread *             net_thread_;
      
      private:
        Tcp::RecvContext *      notification_stop_;
    };
    
    
    /**
     *
     * Sets the Worker-class who handles the business logic of each request,
     * by default one WorkerThread corresponds to one NetThread.
     *
     * The workers' life cycle is managed by Server,
     * just implement WorkerThread's @func HandleImpl and register here.
     *
     */
    template<class WorkerImpl/* : public WorkerThread*/, class ...Args>
    void SetWorker(Args &&..._init_args) {
        if (net_thread_cnt_ <= 0) {
            return;
        }
        for (int i = 0; i < net_thread_cnt_; ++i) {
            auto worker = new WorkerImpl(_init_args...);
            worker_threads_.push_back(worker);
            auto net_thread = net_threads_[i];
            worker->BindNetThread(net_thread);
        }
    }
    
    using RecvQueue = MessageQueue::ThreadSafeDeque<Tcp::RecvContext *>;
    using SendQueue = MessageQueue::ThreadSafeDeque<Tcp::SendContext *>;
    
  private:
    
    class ConnectionManager final {
      public:
        ConnectionManager();
        
        ~ConnectionManager();
        
        void SetEpoll(SocketEpoll *_epoll);
        
        Tcp::ConnectionProfile *GetConnection(SOCKET _fd);
        
        void AddConnection(SOCKET _fd, Tcp::ConnectionProfile *_conn);
        
        void DelConnection(SOCKET _fd);
        
        void ClearTimeout();
        
      private:
        using ScopedLock = std::unique_lock<std::mutex>;
        std::unordered_map<SOCKET, Tcp::ConnectionProfile *> connections_;
        SocketEpoll *                                        socket_epoll_;
        std::mutex                                           mutex_;
    };
    
    
    class NetThread : public Thread {
      public:
        
        NetThread();
        
        ~NetThread() override;
        
        void Run() override;
    
        void NotifySend();
        
        void NotifyStop();
    
        void AddConnection(SOCKET _fd);
    
        void DelConnection(SOCKET _fd);
    
        void HandleSend();
    
        void ClearTimeout();
    
        RecvQueue *GetRecvQueue();
    
        SendQueue *GetSendQueue();
    
        void HandleException(std::exception &ex) override;

      private:
        
        bool __IsNotifySend(const void *_notification) const;
    
        bool __IsNotifyStop(const void *_notification) const;
    
        int __OnReadEvent(SOCKET _fd);
    
        int __OnWriteEvent(Tcp::SendContext *_send_ctx, bool _mod_write);
    
        int __OnErrEvent(SOCKET _fd);
        
        int __OnReadEventTest(SOCKET _fd);
        
      private:
        SocketEpoll             socket_epoll_;
        ConnectionManager       connection_manager_;
        EpollNotifier           epoll_notifier_;
        const void *            notification_send_;
        const void *            notification_stop_;
        RecvQueue               recv_queue_;
        SendQueue               send_queue_;
        
        friend class Server;
    };
    
    void __NotifyWorkerNetThreadsStop();
    
    bool __IsNotifyStop(const void *_notification) const;
    
    int __OnConnect();
    
    void __AddConnection(SOCKET _fd);
    
    int __CreateListenFd();
    
    int __Bind(uint16_t _port) const;
    
    int __HandleErr(SOCKET _fd);
    
  private:
    size_t                      net_thread_cnt_;
    std::vector<NetThread *>    net_threads_;
    std::vector<WorkerThread *> worker_threads_;
    SocketEpoll                 socket_epoll_;
    EpollNotifier               epoll_notifier_;
    const void *                notification_stop_;
    bool                        running_;
    SOCKET                      listenfd_;
    
};


#endif //OI_SVR_SERVER_H
