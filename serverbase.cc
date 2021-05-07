#include "serverbase.h"
#include "signalhandler.h"
#include "log.h"
#include "timeutil.h"



std::string ServerBase::ServerConfigBase::field_port("port");
std::string ServerBase::ServerConfigBase::field_net_thread_cnt("net_thread_cnt");

ServerBase::ServerConfigBase::ServerConfigBase()
        : port(0)
        , net_thread_cnt(0)
        , is_config_done(false) {
}


ServerBase::ServerBase()
        : config_(nullptr)
        , running_(false)
        , listenfd_(INVALID_SOCKET) {
    
}

ServerBase::~ServerBase() {
    delete config_, config_ = nullptr;
}


void ServerBase::BeforeConfig() {
    // implement if needed
}

void ServerBase::Config() {
    
    BeforeConfig();
    
    config_ = _MakeConfig();
    
    assert(config_);
    
    char config_path[128] = {0, };
    snprintf(config_path, sizeof(config_path), "/etc/unixtar/%s", ConfigFile());
    
    yaml::YamlDescriptor config_yaml = yaml::Load(config_path);
    
    if (!config_yaml) {
        LogE("Open %s failed.", ConfigFile())
        assert(false);
    }
    
    do {
        if (yaml::Get(config_yaml, ServerConfigBase::field_port,
                      config_->port) < 0) {
            LogE("Load port from yaml failed.")
            break;
        }
        if (yaml::Get(config_yaml, ServerConfigBase::field_net_thread_cnt,
                      (int &) config_->net_thread_cnt) < 0) {
            LogE("Load net_thread_cnt from yaml failed.")
            break;
        }
        if (config_->net_thread_cnt < 1) {
            LogE("Illegal net_thread_cnt: %zu", config_->net_thread_cnt)
            break;
        }
        if (config_->net_thread_cnt > 8) {
            config_->net_thread_cnt = 8;
        }
        
        if (!_CustomConfig(config_yaml)) {
            LogE("_CustomConfig failed")
            break;
        }
        
        config_->is_config_done = true;
    
    } while (false);
    
    yaml::Close(config_yaml);
    
    if (_CreateListenFd() < 0) {
        LogE("_CreateListenFd failed.")
        assert(false);
    }
    
    if (listenfd_.Bind(AF_INET, config_->port) < 0) {
        LogE("Bind failed.")
        assert(false);
    }
    
    SignalHandler::Instance().RegisterCallback(SIGINT, [this] {
        NotifyStop();
    });
    
    AfterConfig();
}

void ServerBase::AfterConfig() {
    // implement if needed
}

void ServerBase::Serve() {
    
    running_ = true;
    
    ::listen(listenfd_.FD(), 1024);
    
    socket_epoll_.SetListenFd(listenfd_.FD());
    
    epoll_notifier_.SetSocketEpoll(&socket_epoll_);
    
    for (auto &net_thread : net_threads_) {
        net_thread->Start();
    }
    
    while (running_) {
        int n_events = socket_epoll_.EpollWait();
        
        if (n_events < 0) {
            int epoll_errno = socket_epoll_.GetErrNo();
            LogE("break serve, errno(%d): %s",
                 epoll_errno, strerror(epoll_errno))
            running_ = false;
            break;
        }
        
        for (int i = 0; i < n_events; ++i) {
            EpollNotifier::Notification probable_notification(
                    socket_epoll_.GetEpollDataPtr(i));
            
            if (_IsNotifyStop(probable_notification)) {
                LogI("recv notification_stop, break")
                running_ = false;
                break;
            }
            
            if (socket_epoll_.IsNewConnect(i)) {
                _OnConnect();
            }
            
            if (auto fd = (SOCKET) socket_epoll_.IsErrSet(i)) {
                _OnEpollErr(fd);
            }
        }
    }
    
    _NotifyNetThreadsStop();
}

void ServerBase::NotifyStop() {
    LogI("[WebServer::NotifyStop]")
    running_ = false;
    epoll_notifier_.NotifyEpoll(notification_stop_);
}


const uint32_t ServerBase::ConnectionManager::kInvalidUid = 0;
const size_t ServerBase::ConnectionManager::kReserveSize = 128;
const size_t ServerBase::ConnectionManager::kEnlargeUnit = 128;

ServerBase::ConnectionManager::ConnectionManager()
        : socket_epoll_(nullptr) {
    
    ScopedLock lock(mutex_);
    pool_.reserve(kReserveSize);
    pool_.push_back(nullptr);    // pool_[kInvalidUid] not available.
    for (uint32_t i = kInvalidUid + 1; i < kReserveSize; ++i) {
        free_places_.push_back(i);
        pool_.push_back(nullptr);
    }
}

void ServerBase::ConnectionManager::SetEpoll(SocketEpoll *_epoll) {
    if (_epoll) {
        socket_epoll_ = _epoll;
    }
}

tcp::ConnectionProfile *ServerBase::ConnectionManager::GetConnection(uint32_t _uid) {
    assert(_uid > kInvalidUid && _uid < pool_.capacity());
    ScopedLock lock(mutex_);
    return pool_[_uid];
}

void ServerBase::ConnectionManager::AddConnection(tcp::ConnectionProfile *_conn) {
    assert(socket_epoll_);
    assert(_conn);
    
    SOCKET fd = _conn->FD();
    
    __CheckCapacity();
    
    ScopedLock lock(mutex_);
    
    uint32_t uid = free_places_.front();
    free_places_.pop_front();
    
    _conn->SetUid(uid);
    
    if (_conn->GetType() == tcp::ConnectionProfile::kFrom) {
        LogI("fd(%d), from address: [%s:%d], uid: %u", fd,
             _conn->SrcIp().c_str(), _conn->SrcPort(), uid)
    } else {
        LogI("fd(%d), to address: [%s:%d], uid: %u", fd,
             _conn->DstIp().c_str(), _conn->DstPort(), uid)
    }
    
    pool_[uid] = _conn;
    
    // While one thread is blocked in a call to epoll_wait(), it is
    // possible for another thread to add a file descriptor to the
    // waited-upon epoll instance.  If the new file descriptor becomes
    // ready, it will cause the epoll_wait() call to unblock.
    socket_epoll_->AddSocketReadWrite(fd, (uint64_t) _conn);
}

void ServerBase::ConnectionManager::DelConnection(uint32_t _uid) {
    ScopedLock lock(mutex_);
    assert(_uid > kInvalidUid && _uid < pool_.capacity());
    assert(socket_epoll_);
    
    auto &conn = pool_[_uid];
    assert(conn);
    
    SOCKET fd = conn->FD();
    
    socket_epoll_->DelSocket(fd);
    
    free_places_.push_front(_uid);
    
    delete conn, conn = nullptr;
    LogD("fd(%d), uid: %u, free size: %lu", fd, _uid, free_places_.size())
}

void ServerBase::ConnectionManager::ClearTimeout() {
    uint64_t now = ::gettickcount();
    ScopedLock lock(mutex_);
    
    for (auto & conn : pool_) {
        if (conn && conn->IsTimeout(now)) {
            LogI("clear fd: %d", conn->FD())
            socket_epoll_->DelSocket(conn->FD());
            free_places_.push_front(conn->Uid());
            delete conn, conn = nullptr;
        }
    }
}

void ServerBase::ConnectionManager::__CheckCapacity() {
    ScopedLock lock(mutex_);
    if (free_places_.empty()) {
        size_t curr = pool_.capacity();
        pool_.resize(curr + kEnlargeUnit, nullptr);
        for (size_t i = curr; i < pool_.capacity(); ++i) {
            free_places_.push_back(i);
        }
    }
}

ServerBase::ConnectionManager::~ConnectionManager() {
    ScopedLock lock(mutex_);
    for (auto & conn : pool_) {
        if (conn) {
            delete conn, conn = nullptr;
        }
    }
}



ServerBase::NetThreadBase::NetThreadBase()
        : Thread() {
    connection_manager_.SetEpoll(&socket_epoll_);
    epoll_notifier_.SetSocketEpoll(&socket_epoll_);
}

void ServerBase::NetThreadBase::Run() {
    LogD("launching NetThread!")
    int epoll_retry = 3;
    
    const uint64_t clear_timeout_period = 10 * 1000;
    uint64_t last_clear_ts = 0;
    
    while (running_) {
        
        int n_events = socket_epoll_.EpollWait(clear_timeout_period);
        
        if (n_events < 0) {
            if (socket_epoll_.GetErrNo() == EINTR || --epoll_retry > 0) {
                continue;
            }
            break;
        }
        
        for (int i = 0; i < n_events; ++i) {
            EpollNotifier::Notification probable_notification(
                    socket_epoll_.GetEpollDataPtr(i));
    
            if (__IsNotifyStop(probable_notification)) {
                LogI("NetThread notification-stop")
                running_ = false;
                return;
            }
            
            HandleNotification(probable_notification);
            
            tcp::ConnectionProfile *profile;
            if ((profile = (tcp::ConnectionProfile *) socket_epoll_.IsReadSet(i))) {
                _OnReadEvent(profile);
            }
            if ((profile = (tcp::ConnectionProfile *) socket_epoll_.IsWriteSet(i))) {
                auto send_ctx = profile->GetSendContext();
                _OnWriteEvent(send_ctx, true);
            }
            if ((profile = (tcp::ConnectionProfile *) socket_epoll_.IsErrSet(i))) {
                _OnErrEvent(profile);
            }
        }
        
        uint64_t now = ::gettickcount();
        if (now - last_clear_ts > clear_timeout_period) {
            last_clear_ts = now;
            ClearTimeout();
        }
    }
    
}

void ServerBase::NetThreadBase::HandleNotification(EpollNotifier::Notification &) {
    // Implement if needed
}

void ServerBase::NetThreadBase::NotifyStop() {
    running_ = false;
    epoll_notifier_.NotifyEpoll(notification_stop_);
}

void ServerBase::NetThreadBase::RegisterConnection(int _fd, std::string &_ip,
                                                   uint16_t _port) {
    if (_fd < 0) {
        LogE("invalid fd: %d", _fd)
        return;
    }
    
    auto neo = new tcp::ConnectionProfile(ConnectionManager::kInvalidUid,
                                          _fd, _ip, _port);
    connection_manager_.AddConnection(neo);
}

tcp::ConnectionProfile *ServerBase::NetThreadBase::MakeConnection(std::string &_ip,
                                                                  uint16_t _port) {
    auto neo = new tcp::ConnectionProfile(ConnectionManager::kInvalidUid,
                                      _ip, _port);
    if (neo->Connect()) {
        LogE("Connect failed")
    }
    connection_manager_.AddConnection(neo);
    
    return neo;
}

tcp::ConnectionProfile *ServerBase::NetThreadBase::GetConnection(uint32_t _uid) {
    return connection_manager_.GetConnection(_uid);
}

void ServerBase::NetThreadBase::DelConnection(uint32_t _uid) {
    connection_manager_.DelConnection(_uid);
}

void ServerBase::NetThreadBase::ClearTimeout() { connection_manager_.ClearTimeout(); }

int ServerBase::NetThreadBase::_OnErrEvent(tcp::ConnectionProfile *_conn) {
    if (!_conn) {
        return -1;
    }
    LogE("fd(%d), uid: %u", _conn->FD(), _conn->Uid())
    return 0;
}

bool ServerBase::NetThreadBase::__IsNotifyStop(EpollNotifier::Notification &_notification) const {
    return _notification == notification_stop_;
}

ServerBase::NetThreadBase::~NetThreadBase() = default;


ServerBase::ServerConfigBase *ServerBase::_MakeConfig() {
    return new ServerConfigBase();
}

bool ServerBase::_CustomConfig(yaml::YamlDescriptor &_desc) {
    return true;
}

void ServerBase::_NotifyNetThreadsStop() {
    for (auto & net_thread : net_threads_) {
        net_thread->NotifyStop();
        net_thread->Join();
        delete net_thread, net_thread = nullptr;
        LogI("NetThread dead")
    }
    LogI("All Threads Joined!")
}

bool ServerBase::_IsNotifyStop(EpollNotifier::Notification &_notification) const {
    return _notification == notification_stop_;
}

int ServerBase::_OnConnect() {
    SOCKET fd;
    struct sockaddr_in sock_in{};
    socklen_t socklen = sizeof(sockaddr_in);
    char ip_str[INET_ADDRSTRLEN] = {0, };
    uint16_t port;
    
    SOCKET listen_fd = listenfd_.FD();
    while (true) {
        fd = ::accept(listen_fd, (struct sockaddr *) &sock_in, &socklen);
        if (fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (IS_EAGAIN(errno)) {
                return 0;
            }
            LogE("errno(%d): %s", errno, strerror(errno))
            return -1;
        }
        if (!inet_ntop(AF_INET, &sock_in.sin_addr, ip_str, sizeof(ip_str))) {
            LogE("inet_ntop errno(%d): %s", errno, strerror(errno))
        }
        port = ntohs(sock_in.sin_port);
        std::string ip(ip_str);
        
        _RegisterConnection(fd, ip, port);
    }
}

void ServerBase::_RegisterConnection(int _fd, std::string &_ip,
                                     uint16_t _port) {
    if (_fd < 0) {
        LogE("wtf??")
        return;
    }
    uint net_thread_idx = _fd % net_threads_.size();
    NetThreadBase *owner_thread = net_threads_[net_thread_idx];
    if (!owner_thread) {
        LogE("wtf???")
        return;
    }
    owner_thread->RegisterConnection(_fd, _ip, _port);
}

int ServerBase::_CreateListenFd() {
    assert(listenfd_.Create(AF_INET, SOCK_STREAM) >= 0);
    
    // If l_onoff is not 0, The system waits for the length of time
    // described by l_linger for the data in the fd buffer to be sent
    // before the fd actually be destroyed.
    struct linger ling{};
    ling.l_linger = 0;
    ling.l_onoff = 0;   // identical to default.
    listenfd_.SetSocketOpt(SOL_SOCKET, SO_LINGER, &ling, sizeof(ling));
    listenfd_.SetNonblocking();
    return 0;
}

int ServerBase::_OnEpollErr(SOCKET _fd) {
    LogE("fd: %d", _fd)
    return 0;
}

