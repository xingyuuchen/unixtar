#pragma once

#include <deque>
#include <list>
#include <mutex>
#include <vector>
#include <cassert>
#include "thread.h"
#include "networkmodel/tcpconnection.h"
#include "socket/socketepoll.h"
#include "yamlutil.h"



class ServerBase {
  
  public:
    
    ServerBase();
    
    virtual ~ServerBase();
    
    virtual void BeforeConfig();
    
    void Config();
    
    virtual const char *ConfigFile() = 0;
    
    virtual void AfterConfig();
    
    virtual void LoopingEpollWait();
    virtual int EpollLoopInterval();
    
    void Serve();
    
    void NotifyStop();
    
    class ServerConfigBase {
      public:
        ServerConfigBase();
        static const char *const    key_port;
        static const char *const    key_net_thread_cnt;
        static const char *const    key_max_connections;
        uint16_t                    port;
        size_t                      net_thread_cnt;
        size_t                      max_connections;
        bool                        is_config_done;
    };
    
    class ConnectionManager final {
        
        using ScopedLock = std::lock_guard<std::mutex>;
        
      public:
        ConnectionManager();
        
        ~ConnectionManager();
        
        void SetEpoll(SocketEpoll *_epoll);
        
        tcp::ConnectionProfile *GetConnection(uint32_t _uid);
    
        size_t CurrConnectionCnt();
        
        void AddConnection(tcp::ConnectionProfile *);
        
        void DelConnection(uint32_t _uid);
        
        void ClearTimeout();
    
      private:
        void __CheckCapacity();
    
      public:
        static const uint32_t                           kInvalidUid;
      private:
        static const size_t                             kReserveSize;
        static const size_t                             kEnlargeUnit;
        std::vector<tcp::ConnectionProfile *>           pool_;
        std::deque<uint32_t>                            free_places_;
        SocketEpoll *                                   socket_epoll_;
        std::mutex                                      mutex_;
    };
    
    
    class NetThreadBase : public Thread {
      public:
        
        NetThreadBase();
    
        ~NetThreadBase() override;
    
        void Run() final;
    
        /**
         *
         * @return: true if you handled your Notification, else false.
         */
        virtual bool CheckNotification(EpollNotifier::Notification &);
        
        virtual void HandleNotification(EpollNotifier::Notification &);
    
        /**
         *
         * @return: whether such connection is deleted.
         */
        virtual bool HandleApplicationPacket(tcp::RecvContext::Ptr) = 0;
    
        /**
         *
         * @return: whether write event is done.
         */
        static bool TrySendAndMarkPendingIfUndone(const tcp::SendContext::Ptr&);
        
        void NotifyStop();
      
        void RegisterConnection(SOCKET _fd, std::string &_ip, uint16_t _port);
    
        virtual void ConfigApplicationLayer(tcp::ConnectionProfile *) = 0;
    
        virtual void UpgradeApplicationProtocol(tcp::ConnectionProfile *,
                                                const tcp::RecvContext::Ptr&);
    
        void SetMaxConnection(size_t);
        
        tcp::ConnectionProfile *MakeConnection(std::string &_ip, uint16_t _port);
    
        tcp::ConnectionProfile *GetConnection(uint32_t _uid);
        
        void DelConnection(uint32_t _uid);
    
        void ClearTimeout();

      private:
        
        /**
         *
         * @return: whether @param{_conn} is deleted.
         */
        bool __OnReadEvent(tcp::ConnectionProfile *_conn);
    
        static void __OnWriteEvent(tcp::ConnectionProfile *);
    
        virtual int __OnErrEvent(tcp::ConnectionProfile *);
        
        bool __IsNotifyStop(EpollNotifier::Notification &) const;

      private:
        SocketEpoll                         socket_epoll_;
        ConnectionManager                   connection_manager_;
        EpollNotifier::Notification         notification_stop_;
      protected:
        EpollNotifier                       epoll_notifier_;
        size_t                              max_connections_;
    };
    
    
    template<class NetThreadImpl /* : public NetThreadBase*/, class ...Args>
    void
    SetNetThreadImpl(Args &&... _init_args) {
        assert(config_ && config_->is_config_done);
        for (int i = 0; i < config_->net_thread_cnt; ++i) {
            auto net_thread = new NetThreadImpl(_init_args...);
            net_threads_.push_back(net_thread);
        }
    }

  protected:
    
    virtual ServerConfigBase *_MakeConfig();
    
    virtual bool _CustomConfig(yaml::YamlDescriptor *_desc);
    
    void _NotifyNetThreadsStop();
    
    bool _IsNotifyStop(EpollNotifier::Notification &) const;
    
    int _OnConnect();
    
    void _RegisterConnection(SOCKET _fd, std::string &_ip,
                             uint16_t _port);
    
    int _CreateListenFd();
    
    virtual int _OnEpollErr(SOCKET);

  protected:
    ServerConfigBase                  * config_;
    std::vector<NetThreadBase *>        net_threads_;
    SocketEpoll                         socket_epoll_;
    EpollNotifier                       epoll_notifier_;
    EpollNotifier::Notification         notification_stop_;
    bool                                running_;
    Socket                              listenfd_;
    static const size_t                 kDefaultListenBacklog;
    
};

