#include "serverbase.h"
#include "signalhandler.h"
#include "log.h"
#include "timeutil.h"
#include <unistd.h>



const size_t ServerBase::kDefaultListenBacklog = 4096;

const char *const ServerBase::ServerConfigBase::key_port("port");
const char *const ServerBase::ServerConfigBase::key_net_thread_cnt("net_thread_cnt");
const char *const ServerBase::ServerConfigBase::key_max_connections("max_connections");

ServerBase::ServerConfigBase::ServerConfigBase()
        : port(0)
        , net_thread_cnt(0)
        , max_connections(0)
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
    
    yaml::YamlDescriptor *config_yaml = yaml::Load(config_path);
    
    if (!config_yaml) {
        LogE("Open %s failed.", ConfigFile())
        assert(false);
    }
    
    do {
        try {
            yaml::ValueLeaf *port = config_yaml->GetLeaf(ServerConfigBase::key_port);
            port->To(config_->port);
    
            yaml::ValueLeaf *net_thread_cnt = config_yaml->GetLeaf(
                    ServerConfigBase::key_net_thread_cnt);
            net_thread_cnt->To((int &) config_->net_thread_cnt);
    
            yaml::ValueLeaf *max_connections = config_yaml->GetLeaf(
                    ServerConfigBase::key_max_connections);
            max_connections->To((int &) config_->max_connections);
            
        } catch (std::exception &exception) {
            LogE("catch yaml exception: %s", exception.what())
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
    
    assert(_CreateListenFd() >= 0);
    
    assert(listenfd_.Bind(AF_INET, config_->port) >= 0);
    
    
    SignalHandler::Instance().RegisterCallback(SIGINT, [this] {
        NotifyStop();
    });
    
    AfterConfig();
}

void ServerBase::AfterConfig() {
    // implement if needed
}
void ServerBase::LoopingEpollWait() {
    // implement if needed
}

int ServerBase::EpollLoopInterval() {
    return 30 * 1000;
}

void ServerBase::Serve() {
    
    running_ = true;
    
    assert(listenfd_.Listen(kDefaultListenBacklog) >= 0);
    
    socket_epoll_.SetListenFd(listenfd_.FD());
    
    epoll_notifier_.SetSocketEpoll(&socket_epoll_);
    
    for (auto &net_thread : net_threads_) {
        net_thread->Start();
    }
    const int kEpollWaitInterval = EpollLoopInterval();
    uint64_t last_invoke = 0;
    
    while (running_) {
        int n_events = socket_epoll_.EpollWait(kEpollWaitInterval);
        
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
            
            if (auto fd = (SOCKET) socket_epoll_.IsErrSet(i)) {
                _OnEpollErr(fd);
                continue;
            }

            if (socket_epoll_.IsNewConnect(i)) {
                _OnConnect();
            }
        }
        
        uint64_t now = ::gettickcount();
        if (now - last_invoke > kEpollWaitInterval) {
            LoopingEpollWait();
            last_invoke = now;
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
    
    if (_conn->GetType() == tcp::TConnectionType::kAcceptFrom) {
        LogI("fd(%d), from: [%s:%d], uid: %u", fd,
             _conn->RemoteIp().c_str(), _conn->RemotePort(), uid)
    } else {
        LogI("fd(%d), to: [%s:%d], uid: %u", fd,
             _conn->RemoteIp().c_str(), _conn->RemotePort(), uid)
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
            
            if (HandleNotification(probable_notification)) {
                continue;
            }
            
            tcp::ConnectionProfile *tcp_conn;
            
            if ((tcp_conn = (tcp::ConnectionProfile *) socket_epoll_.IsErrSet(i))) {
                __OnErrEvent(tcp_conn);
                continue;
            }
            
            if ((tcp_conn = (tcp::ConnectionProfile *) socket_epoll_.IsReadSet(i))) {
                bool deleted = __OnReadEvent(tcp_conn);
                if (deleted) {
                    continue;
                }
            }
            
            if ((tcp_conn = (tcp::ConnectionProfile *) socket_epoll_.IsWriteSet(i))) {
                __OnWriteEvent(tcp_conn);
            }
        }
        
        uint64_t now = ::gettickcount();
        if (now - last_clear_ts > clear_timeout_period) {
            last_clear_ts = now;
            ClearTimeout();
        }
    }
    
}

bool ServerBase::NetThreadBase::HandleNotification(EpollNotifier::Notification &) {
    return false;
}

void ServerBase::NetThreadBase::UpgradeApplicationProtocol(tcp::ConnectionProfile *,
                                                           const tcp::RecvContext::Ptr&) {
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
    
    auto neo = new tcp::ConnectionFrom(_fd, _ip, _port,
                                       ConnectionManager::kInvalidUid);
    connection_manager_.AddConnection(neo);
    
    ConfigApplicationLayer(neo);
}

tcp::ConnectionProfile *ServerBase::NetThreadBase::MakeConnection(std::string &_ip,
                                                                  uint16_t _port) {
    auto neo = new tcp::ConnectionTo(_ip, _port,
                                     ConnectionManager::kInvalidUid);
    
    int retry = 3;
    bool success = false;
    for (int i = 0; i < retry; ++i) {
        if (neo->Connect() < 0) {
            LogE("connect failed, retry %d time...", i)
            continue;
        }
        LogI("connect to [%s:%d] succeed, fd(%d)", _ip.c_str(), _port, neo->FD())
        success = true;
        break;
    }
    if (success) {
        connection_manager_.AddConnection(neo);
        ConfigApplicationLayer(neo);
        return neo;
    }
    delete neo;
    return nullptr;
}

tcp::ConnectionProfile *ServerBase::NetThreadBase::GetConnection(uint32_t _uid) {
    return connection_manager_.GetConnection(_uid);
}

void ServerBase::NetThreadBase::DelConnection(uint32_t _uid) {
    connection_manager_.DelConnection(_uid);
}

void ServerBase::NetThreadBase::ClearTimeout() { connection_manager_.ClearTimeout(); }


bool ServerBase::NetThreadBase::__OnReadEvent(tcp::ConnectionProfile *_conn) {
    assert(_conn);
    
    SOCKET fd = _conn->FD();
    uint32_t uid = _conn->Uid();
    
    if (_conn->Receive() < 0) {
        if (!_conn->HasReceivedFIN()) {
            LogE("fd(%d), uid: %d Receive() err, _conn: %p", _conn->FD(), _conn->Uid(), _conn)
        }
        DelConnection(uid);
        return true;
    }
    
    if (_conn->IsParseDone()) {
        LogI("fd(%d) %s parse succeed", fd, _conn->ApplicationProtocolName())
        
        bool is_upgrade_app_proto = _conn->IsUpgradeApplicationProtocol();
        tcp::RecvContext::Ptr recv_ctx = _conn->MakeRecvContext();
    
        if (_conn->GetType() == tcp::TConnectionType::kAcceptFrom
                        && !is_upgrade_app_proto) {
            // only requests need a packet to send back.
            recv_ctx->packet_back = _conn->MakeSendContext();
        }
        
        if (is_upgrade_app_proto) {
            LogI("upgrade application protocol")
            UpgradeApplicationProtocol(_conn, recv_ctx);
            // after upgrading protocol, the request need a packet
            // to send back handshake, etc.
            recv_ctx = _conn->MakeRecvContext(true);
        }
        
        return HandleApplicationPacket(recv_ctx);
    }
    return false;
}

void ServerBase::NetThreadBase::__OnWriteEvent(tcp::ConnectionProfile *_conn) {
    if (_conn->HasPendingPacketToSend()) {
        bool write_done = _conn->TrySendPendingPackets();
        if (!_conn->IsLongLinkApplicationProtocol() && write_done) {
            DelConnection(_conn->Uid());
        }
    }
}

bool ServerBase::NetThreadBase::TrySendAndMarkPendingIfUndone(
                const tcp::SendContext::Ptr& _send_ctx) {
    bool is_send_done = tcp::ConnectionProfile::TrySend(_send_ctx);
    if (!is_send_done) {
        // Mark self as pending so that epoll will
        // continue to notify sending if it's possible.
        _send_ctx->MarkAsPendingPacket();
    }
    return is_send_done;
}

int ServerBase::NetThreadBase::__OnErrEvent(tcp::ConnectionProfile *_conn) {
    if (!_conn) {
        return -1;
    }
    uint32_t uid = _conn->Uid();
    LogE("fd(%d), uid: %u, _conn: %p", _conn->FD(), uid, _conn)
    DelConnection(uid);
    return 0;
}

bool ServerBase::NetThreadBase::__IsNotifyStop(EpollNotifier::Notification &_notification) const {
    return _notification == notification_stop_;
}

ServerBase::NetThreadBase::~NetThreadBase() = default;


ServerBase::ServerConfigBase *ServerBase::_MakeConfig() {
    return new ServerConfigBase();
}

bool ServerBase::_CustomConfig(yaml::YamlDescriptor *_desc) {
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

