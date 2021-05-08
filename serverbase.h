#pragma once

#include <cstdint>
#include <deque>
#include <list>
#include <mutex>
#include <vector>
#include <cassert>
#include "thread.h"
#include "http/connectionprofile.h"
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
    
    virtual void Serve();
    
    void NotifyStop();
    
    class ServerConfigBase {
      public:
        ServerConfigBase();
        static std::string      field_port;
        uint16_t                port;
        static std::string      field_net_thread_cnt;
        size_t                  net_thread_cnt;
        bool                    is_config_done;
    };
    
    class ConnectionManager final {
        
        using ScopedLock = std::lock_guard<std::mutex>;
        
      public:
        ConnectionManager();
        
        ~ConnectionManager();
        
        void SetEpoll(SocketEpoll *_epoll);
        
        tcp::ConnectionProfile *GetConnection(uint32_t _uid);
        
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
    
        void Run() override;
    
        /**
         *
         * @return: true if you handled your Notification, else false.
         */
        virtual bool HandleNotification(EpollNotifier::Notification &);
        
        void NotifyStop();
      
        void RegisterConnection(SOCKET _fd, std::string &_ip, uint16_t _port);
    
        tcp::ConnectionProfile *MakeConnection(std::string &_ip, uint16_t _port);
    
        tcp::ConnectionProfile *GetConnection(uint32_t _uid);
        
        void DelConnection(uint32_t _uid);
    
        void ClearTimeout();

      protected:
        
        virtual int _OnReadEvent(tcp::ConnectionProfile *) = 0;
    
        virtual int _OnWriteEvent(tcp::SendContext *, bool _del) = 0;
    
        virtual int _OnErrEvent(tcp::ConnectionProfile *);
        
      private:
        bool __IsNotifyStop(EpollNotifier::Notification &) const;

      protected:
        EpollNotifier                       epoll_notifier_;
      private:
        SocketEpoll                         socket_epoll_;
        ConnectionManager                   connection_manager_;
        EpollNotifier::Notification         notification_stop_;
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
    
    virtual bool _CustomConfig(yaml::YamlDescriptor &_desc);
    
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
    
};

