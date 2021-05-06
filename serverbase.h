#pragma once

#include <cstdint>
#include <deque>
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
#include "yamlutil.h"



class ServerBase {
  
  public:
    
    ServerBase();
    
    ~ServerBase();
    
    virtual void BeforeConfig();
    
    void Config();
    
    virtual void AfterConfig();
    
    virtual void Serve();
    
    void NotifyStop();
    
    class ServerConfigBase {
      public:
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
        
        void AddConnection(SOCKET _fd, std::string &_ip, uint16_t _port);
        
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
    
        virtual void HandleNotification(EpollNotifier::Notification &) = 0;
        
        void NotifyStop();
      
        void AddConnection(SOCKET _fd, std::string &_ip, uint16_t _port);
      
        void DelConnection(uint32_t _uid);
    
        void ClearTimeout();

      protected:
        
        virtual int __OnReadEvent(tcp::ConnectionProfile *) = 0;
    
        virtual int __OnWriteEvent(tcp::SendContext *) = 0;
    
        virtual int __OnErrEvent(tcp::ConnectionProfile *) = 0;
        
        bool _IsNotifyStop(EpollNotifier::Notification &) const;

      protected:
        SocketEpoll                         socket_epoll_;
        ConnectionManager                   connection_manager_;
        EpollNotifier                       epoll_notifier_;
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
    
    virtual ServerConfigBase *MakeConfig();
    
    virtual bool _DoConfig(yaml::YamlDescriptor &_desc);
    
    void _NotifyNetThreadsStop();
    
    bool _IsNotifyStop(EpollNotifier::Notification &) const;
    
    int _OnConnect();
    
    void _AddConnection(SOCKET _fd, std::string &_ip, uint16_t _port);
    
    int _CreateListenFd();
    
    virtual int _OnEpollErr(SOCKET) = 0;

  protected:
    ServerConfigBase                  * config_;
    std::vector<NetThreadBase *>        net_threads_;
    SocketEpoll                         socket_epoll_;
    EpollNotifier                       epoll_notifier_;
    EpollNotifier::Notification         notification_stop_;
    bool                                running_;
    Socket                              listenfd_;
    
};

