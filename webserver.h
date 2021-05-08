#pragma once

#include "httpserver.h"
#include "netscenebase.h"
#include "messagequeue.h"
#include "singleton.h"


class WebServer final : public HttpServer {
    
    SINGLETON(WebServer, )
    
    class NetThread;

  public:
    
    void AfterConfig() override;
    
    const char *ConfigFile() override;
    
    ~WebServer() override;
    
    class ServerConfig : public ServerBase::ServerConfigBase {
      public:
        ServerConfig();
        static std::string      field_max_backlog;
        size_t                  max_backlog;
        static std::string      field_worker_thread_cnt;
        size_t                  worker_thread_cnt;
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
        
        auto conf = (ServerConfig *) config_;
        assert(conf->is_config_done);
        
        for (int i = 0; i < conf->worker_thread_cnt; ++i) {
            auto worker = new WorkerImpl(_init_args...);
            size_t idx = i % conf->net_thread_cnt;
            auto net_thread = net_threads_[idx];
            ((NetThread *) net_thread)->BindNewWorker(worker);
        }
    }
    
    using RecvQueue = MessageQueue::ThreadSafeDeque<http::RecvContext *>;
    using SendQueue = MessageQueue::ThreadSafeDeque<tcp::SendContext *>;
    
  private:
    
    class NetThread : public HttpNetThread {
      public:
        
        NetThread();
        
        ~NetThread() override;
        
        bool IsWorkerOverload();
        
        bool IsWorkerFullyLoad();
    
        size_t GetMaxBacklog() const;
    
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
    
        int HandleHttpPacket(tcp::ConnectionProfile *) override;

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
    
    bool _CustomConfig(yaml::YamlDescriptor &_desc) override;
    
  private:
    static const char* const kConfigFile;
};

