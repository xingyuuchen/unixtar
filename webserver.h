#pragma once

#include "serverbase.h"
#include "netscenebase.h"
#include "messagequeue.h"
#include "singleton.h"


class WebServer final : public ServerBase {
    
    SINGLETON(WebServer, )
    
    class NetThread;

  public:
    
    void AfterConfig() override;
    
    const char *ConfigFile() override;
    
    void LoopingEpollWait() override;
    
    int EpollLoopInterval() override;
    
    void SendHeartbeat();
    
    ~WebServer() override;
    
    class ServerConfig : public ServerBase::ServerConfigBase {
      public:
        ServerConfig();
        static const char *const    key_max_backlog;
        static const char *const    key_worker_thread_cnt;
        static const char *const    key_reverse_proxy;
        static const char *const    key_ip;
        static const char *const    key_heartbeat_period;
        size_t                      max_backlog;
        size_t                      worker_thread_cnt;
        std::string                 reverse_proxy_ip;
        uint16_t                    reverse_proxy_port;
        int                         heartbeat_period;
    };

    class WorkerThread : public Thread {
      public:
        WorkerThread();
        
        void Run() final;
    
        virtual void HandleImpl(tcp::RecvContext *) = 0;
        
        virtual void HandleOverload(tcp::RecvContext *) = 0;
    
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
        
        auto conf = (ServerConfig *) config_;
        assert(conf->is_config_done);
        
        for (int i = 0; i < conf->worker_thread_cnt; ++i) {
            auto worker = new WorkerImpl(_init_args...);
            size_t idx = i % conf->net_thread_cnt;
            auto net_thread = net_threads_[idx];
            ((NetThread *) net_thread)->BindNewWorker(worker);
        }
    }
    
    using RecvQueue = MessageQueue::ThreadSafeDeque<tcp::RecvContext *>;
    using SendQueue = MessageQueue::ThreadSafeDeque<tcp::SendContext *>;
    
  private:
    
    class NetThread : public NetThreadBase {
      public:
        
        NetThread();
        
        ~NetThread() override;
        
        bool IsWorkerOverload();
        
        bool IsWorkerFullyLoad();
    
        size_t GetMaxBacklog() const;
        
        size_t Backlog();
    
        /**
         * @param _backlog: The maximum backlog for waiting queue (which holds parsed HTTP packets
                            but have not yet been processed by the WorkerThread).
         */
        void SetMaxBacklog(size_t _backlog);
    
        void NotifySend();
        
        bool HandleNotification(EpollNotifier::Notification &) override;
    
        void HandleSend();
    
        RecvQueue *GetRecvQueue();
    
        SendQueue *GetSendQueue();
        
        void BindNewWorker(WorkerThread *);
        
        void OnStart() override;
    
        void OnStop() override;
    
        void ConfigApplicationLayer(tcp::ConnectionProfile *) override;
    
        int HandleApplicationPacket(tcp::ConnectionProfile *) override;

        void HandleException(std::exception &ex) override;

      private:
        void __NotifyWorkersStop();
        
        bool __IsNotifySend(EpollNotifier::Notification &) const;
        
      private:
        EpollNotifier::Notification         notification_send_;
        RecvQueue                           recv_queue_;
        size_t                              max_backlog_;
        static const size_t                 kDefaultMaxBacklog;
        SendQueue                           send_queue_;
        std::list<WorkerThread *>           workers_;
        
        friend class WebServer;
    };
    
  protected:
    ServerConfigBase *_MakeConfig() override;
    
    bool _CustomConfig(yaml::YamlDescriptor *_desc) override;
    
  private:
    static const char* const    kConfigFile;
    static const int            kDefaultHeartBeatPeriod;
};

