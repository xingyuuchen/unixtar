#include "webserver.h"
#include "utils/log.h"
#include "socketepoll.h"
#include "yamlutil.h"
#include "signalhandler.h"
#include "timeutil.h"
#include <unistd.h>
#include <cstring>


std::string WebServer::ServerConfig::field_port("port");
std::string WebServer::ServerConfig::field_net_thread_cnt("net_thread_cnt");
std::string WebServer::ServerConfig::field_max_backlog("max_backlog");
std::string WebServer::ServerConfig::field_worker_thread_cnt("worker_thread_cnt");

uint16_t WebServer::ServerConfig::port = 0;
size_t WebServer::ServerConfig::net_thread_cnt = 0;
size_t WebServer::ServerConfig::max_backlog = 0;
size_t WebServer::ServerConfig::worker_thread_cnt = 0;
bool WebServer::ServerConfig::is_config_done = false;


WebServer::WebServer()
        : running_(false)
        , listenfd_(INVALID_SOCKET) {
    
    yaml::YamlDescriptor server_config = yaml::Load("../framework/serverconfig.yml");
    
    if (!server_config) {
        LogE("Open serverconfig.yaml failed.")
        assert(false);
    }
    
    do {
        if (yaml::Get(server_config, ServerConfig::field_port,
                      ServerConfig::port) < 0) {
            LogE("Load port from yaml failed.")
            break;
        }
        if (yaml::Get(server_config, ServerConfig::field_net_thread_cnt,
                         (int &) ServerConfig::net_thread_cnt) < 0) {
            LogE("Load net_thread_cnt from yaml failed.")
            break;
        }
        if (yaml::Get(server_config, ServerConfig::field_max_backlog,
                      (int &) ServerConfig::max_backlog) < 0) {
            LogE("Load max_backlog from yaml failed.")
            break;
        }
        if (yaml::Get(server_config, ServerConfig::field_worker_thread_cnt,
                      (int &) ServerConfig::worker_thread_cnt) < 0) {
            LogE("Load worker_thread_cnt from yaml failed.")
            break;
        }
        if (ServerConfig::net_thread_cnt < 1) {
            LogE("Illegal net_thread_cnt: %zu", ServerConfig::net_thread_cnt)
            break;
        }
        if (ServerConfig::net_thread_cnt > 8) {
            ServerConfig::net_thread_cnt = 8;
        }
        if (ServerConfig::worker_thread_cnt < 1) {
            LogE("Illegal worker_thread_cnt: %zu", ServerConfig::worker_thread_cnt)
            break;
        }
        if (ServerConfig::worker_thread_cnt % ServerConfig::net_thread_cnt != 0) {
            LogW("Config worker_thread_cnt as an integer multiple time of "
                 "net_thread_cnt to get best performance.")
        }
        if (ServerConfig::worker_thread_cnt > 4 * ServerConfig::net_thread_cnt) {
            LogW("Excessive proportion of worker_thread / net_thread "
                 "may lower performance of net_thread.")
        }
        if (ServerConfig::max_backlog < 0) {
            LogE("Please config max_backlog a positive number.")
            break;
        }
        
        if (__CreateListenFd() < 0) { break; }
        
        if (listenfd_.Bind(AF_INET, ServerConfig::port) < 0) { break; }
        
        for (int i = 0; i < ServerConfig::net_thread_cnt; ++i) {
            auto net_thread = new NetThread();
            net_thread->SetMaxBacklog(ServerConfig::max_backlog);
            net_threads_.push_back(net_thread);
        }
        
        ServerConfig::is_config_done = true;
        
    } while (false);
    
    yaml::Close(server_config);
    
}


void WebServer::Serve() {
    if (!ServerConfig::is_config_done) {
        LogE("config me first!")
        assert(false);
    }
    
    SignalHandler::Instance().RegisterCallback(SIGINT, [this] {
        NotifyStop();
    });
    
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
            
            if (__IsNotifyStop(probable_notification)) {
                LogI("recv notification_stop, break")
                running_ = false;
                break;
            }
            
            if (socket_epoll_.IsNewConnect(i)) {
                __OnConnect();
            }
            
            if (auto fd = (SOCKET) socket_epoll_.IsErrSet(i)) {
                __OnEpollErr(fd);
            }
        }
    }
    
    __NotifyNetThreadsStop();
}

void WebServer::NotifyStop() {
    LogI("[WebServer::NotifyStop]")
    running_ = false;
    epoll_notifier_.NotifyEpoll(notification_stop_);
}

WebServer::~WebServer() = default;

WebServer::WorkerThread::WorkerThread()
        : net_thread_(nullptr)
        , thread_seq_(__MakeWorkerThreadSeq()) {
    
}

void WebServer::WorkerThread::Run() {
    if (!net_thread_) {
        LogE("WorkerThread bind NetThread first!")
        running_ = false;
        return;
    }
    LogI("Launching WorkerThread %d", thread_seq_)
    
    auto recv_queue = net_thread_->GetRecvQueue();
    
    while (true) {
        
        http::RecvContext *recv_ctx;
        if (recv_queue->pop_front_to(recv_ctx)) {
        
            if (net_thread_->IsWorkerOverload()) {
                HandleOverload(recv_ctx);
            } else {
                HandleImpl(recv_ctx);
            }
            tcp::SendContext *send_ctx = recv_ctx->send_context;
            net_thread_->GetSendQueue()->push_back(send_ctx);
            net_thread_->NotifySend();
            continue;
        }
        
        if (recv_queue->IsTerminated()) {
            running_ = false;
            size_t left = recv_queue->size();
            if (left > 0) {
                LogI("WorkerThread %zu tasks left", left)
            }
            break;
        }
    }
    
    LogI("Worker%d terminate!", thread_seq_)
}

void WebServer::WorkerThread::BindNetThread(WebServer::NetThread *_net_thread) {
    if (_net_thread) {
        net_thread_ = _net_thread;
    }
}

void WebServer::WorkerThread::NotifyStop() {
    if (!net_thread_) {
        LogE("!net_thread_, notify failed")
        return;
    }
    LogI("notify worker%d stop", thread_seq_)
    
    net_thread_->GetRecvQueue()->Terminate();
}

int WebServer::WorkerThread::__MakeWorkerThreadSeq() {
    static int thread_seq = 0;
    return ++thread_seq;
}

int WebServer::WorkerThread::GetWorkerSeqNum() const { return thread_seq_; }

WebServer::WorkerThread::~WorkerThread() = default;



const size_t WebServer::ConnectionManager::kReserveSize = 128;
const size_t WebServer::ConnectionManager::kEnlargeUnit = 128;

WebServer::ConnectionManager::ConnectionManager()
        : socket_epoll_(nullptr) {
    
    pool_.reserve(kReserveSize);
    for (uint32_t i = 0; i < kReserveSize; ++i) {
        free_places_.push(i);
        pool_.push_back(nullptr);
    }
}

void WebServer::ConnectionManager::SetEpoll(SocketEpoll *_epoll) {
    if (_epoll) {
        socket_epoll_ = _epoll;
    }
}

tcp::ConnectionProfile *WebServer::ConnectionManager::GetConnection(uint32_t _uid) {
    assert(_uid >= 0 && _uid < pool_.capacity());
    ScopedLock lock(mutex_);
    return pool_[_uid];
}

void WebServer::ConnectionManager::AddConnection(SOCKET _fd,
                                                 std::string &_ip,
                                                 uint16_t _port) {
    if (_fd < 0) {
        LogE("fd(%d)", _fd)
        LogPrintStacktrace()
        return;
    }
    assert(socket_epoll_);
    
    __CheckCapacity();
    
    ScopedLock lock(mutex_);
    
    uint32_t uid = free_places_.front();
    free_places_.pop();
    
    LogI("fd(%d), address: [%s:%d], uid: %u", _fd, _ip.c_str(), _port, uid)
    
    auto neo = new tcp::ConnectionProfile(uid, _fd, _ip, _port);
    pool_[uid] = neo;
    
    // While one thread is blocked in a call to epoll_wait(), it is
    // possible for another thread to add a file descriptor to the
    // waited-upon epoll instance.  If the new file descriptor becomes
    // ready, it will cause the epoll_wait() call to unblock.
    socket_epoll_->AddSocketReadWrite(_fd, (uint64_t) neo);
}

void WebServer::ConnectionManager::DelConnection(uint32_t _uid) {
    ScopedLock lock(mutex_);
    assert(_uid >= 0 && _uid < pool_.capacity());
    
    auto &conn = pool_[_uid];
    if (socket_epoll_) {
        socket_epoll_->DelSocket(conn->FD());
    }
    delete conn, conn = nullptr;
    
    free_places_.push(_uid);
    
    LogD("free size: %lu", free_places_.size())
}

void WebServer::ConnectionManager::ClearTimeout() {
    uint64_t now = ::gettickcount();
    ScopedLock lock(mutex_);
    
    for (auto & conn : pool_) {
        if (conn && conn->IsTimeout(now)) {
            LogI("clear fd: %d", conn->FD())
            socket_epoll_->DelSocket(conn->FD());
            free_places_.push(conn->Uid());
            delete conn, conn = nullptr;
        }
    }
}

void WebServer::ConnectionManager::__CheckCapacity() {
    ScopedLock lock(mutex_);
    if (free_places_.empty()) {
        size_t curr = pool_.size();
        size_t neo = curr + kEnlargeUnit;
        pool_.resize(neo, nullptr);
        for (size_t i = curr; i < neo; ++i) {
            free_places_.push(i);
        }
    }
}

WebServer::ConnectionManager::~ConnectionManager() {
    ScopedLock lock(mutex_);
    for (auto & conn : pool_) {
        if (conn) {
            delete conn, conn = nullptr;
        }
    }
}


const size_t WebServer::NetThread::kDefaultMaxBacklog = 1024;

WebServer::NetThread::NetThread()
        : Thread()
        , max_backlog_(kDefaultMaxBacklog) {
    
    connection_manager_.SetEpoll(&socket_epoll_);
    epoll_notifier_.SetSocketEpoll(&socket_epoll_);
}

void WebServer::NetThread::Run() {
    LogD("launching NetThread!")
    int epoll_retry = 3;
    
    const uint64_t clear_timeout_period = 10 * 1000;
    uint64_t last_clear_ts = 0;
    
    while (true) {
        
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
            
            if (__IsNotifySend(probable_notification)) {
                HandleSend();
                continue;
                
            } else if (__IsNotifyStop(probable_notification)) {
                LogI("NetThread notification-stop, break")
                running_ = false;
                __NotifyWorkersStop();
                return;
            }
            
            tcp::ConnectionProfile *profile;
            if ((profile = (tcp::ConnectionProfile *) socket_epoll_.IsReadSet(i))) {
                __OnReadEvent(profile);
            }
            if ((profile = (tcp::ConnectionProfile *) socket_epoll_.IsWriteSet(i))) {
                auto send_ctx = profile->GetSendContext();
                __OnWriteEvent(send_ctx);
            }
            if ((profile = (tcp::ConnectionProfile *) socket_epoll_.IsErrSet(i))) {
                __OnErrEvent(profile);
            }
        }
    
        uint64_t now = ::gettickcount();
        if (now - last_clear_ts > clear_timeout_period) {
            last_clear_ts = now;
            ClearTimeout();
        }
    }
    
}

bool WebServer::NetThread::IsWorkerOverload() { return recv_queue_.size() > max_backlog_ * 9 / 10; }

bool WebServer::NetThread::IsWorkerFullyLoad() { return recv_queue_.size() >= max_backlog_; }

size_t WebServer::NetThread::GetMaxBacklog() const { return max_backlog_; }

void WebServer::NetThread::SetMaxBacklog(size_t _backlog) { max_backlog_ = _backlog; }

void WebServer::NetThread::NotifySend() {
    epoll_notifier_.NotifyEpoll(notification_send_);
}

void WebServer::NetThread::NotifyStop() {
    running_ = false;
    epoll_notifier_.NotifyEpoll(notification_stop_);
}

void WebServer::NetThread::AddConnection(SOCKET _fd, std::string &_ip, uint16_t _port) {
    if (_fd < 0) {
        LogE("invalid fd: %d", _fd)
        return;
    }
    connection_manager_.AddConnection(_fd, _ip, _port);
}

void WebServer::NetThread::DelConnection(uint32_t _uid) {
    connection_manager_.DelConnection(_uid);
}

void WebServer::NetThread::HandleSend() {
    tcp::SendContext *send_ctx;
    while (send_queue_.pop_front_to(send_ctx, false)) {
        LogD("fd(%d) doing send task", send_ctx->fd)
        __OnWriteEvent(send_ctx);
    }
}

void WebServer::NetThread::ClearTimeout() { connection_manager_.ClearTimeout(); }

WebServer::RecvQueue *WebServer::NetThread::GetRecvQueue() { return &recv_queue_; }

WebServer::SendQueue *WebServer::NetThread::GetSendQueue() { return &send_queue_; }

void WebServer::NetThread::BindNewWorker(WebServer::WorkerThread *_worker) {
    if (_worker) {
        workers_.emplace_back(_worker);
        _worker->BindNetThread(this);
    }
}

void WebServer::NetThread::HandleException(std::exception &ex) {
    LogE("%s", ex.what())
}

bool WebServer::NetThread::__IsNotifySend(EpollNotifier::Notification &_notification) const {
    return _notification == notification_send_;
}

bool WebServer::NetThread::__IsNotifyStop(EpollNotifier::Notification &_notification) const {
    return _notification == notification_stop_;
}

int WebServer::NetThread::__OnReadEvent(tcp::ConnectionProfile * _conn) {
    assert(_conn);
    
    SOCKET fd = _conn->FD();
    uint32_t uid = _conn->Uid();
    LogI("fd(%d), uid: %u", fd, uid)
    
    if (_conn->Receive() < 0) {
        DelConnection(uid);
        return -1;
    }
    
    if (_conn->IsParseDone()) {
        LogI("fd(%d) http parse succeed", fd)
        if (IsWorkerFullyLoad()) {
            LogI("worker fully loaded, drop connection directly")
            DelConnection(uid);
            
        } else {
            recv_queue_.push_back(_conn->GetRecvContext());
        }
    }
    return 0;
}

int WebServer::NetThread::__OnWriteEvent(tcp::SendContext *_send_ctx) {
    if (!_send_ctx) {
        LogE("!_send_ctx")
        return -1;
    }
    AutoBuffer &resp = _send_ctx->buffer;
    size_t pos = resp.Pos();
    size_t ntotal = resp.Length() - pos;
    SOCKET fd = _send_ctx->fd;
    
    if (fd < 0) {
        return 0;
    }
    if (ntotal == 0) {
        LogI("fd(%d), nothing to send", fd)
        return 0;
    }
    
    ssize_t nsend = ::write(fd, resp.Ptr(pos), ntotal);
    
    do {
        if (nsend == ntotal) {
            LogI("fd(%d), send %zd/%zu B, done", fd, nsend, ntotal)
            break;
        }
        if (nsend >= 0 || (nsend < 0 && IS_EAGAIN(errno))) {
            nsend = nsend > 0 ? nsend : 0;
            LogI("fd(%d): send %zd/%zu B", fd, nsend, ntotal)
            resp.Seek(pos + nsend);
            return 0;
        }
        if (nsend < 0) {
            if (errno == EPIPE) {
                // fd probably closed by peer, or cleared because of timeout.
                LogI("fd(%d) already closed, send nothing", fd)
                return 0;
            }
            LogE("fd(%d) nsend(%zd), errno(%d): %s",
                 fd, nsend, errno, strerror(errno));
        }
    } while (false);
    
    DelConnection(_send_ctx->connection_uid);
    
    return nsend < 0 ? -1 : 0;
}

int WebServer::NetThread::__OnErrEvent(tcp::ConnectionProfile *_profile) {
    if (!_profile) {
        return -1;
    }
    LogE("fd: %d, uid: %u", _profile->FD(), _profile->Uid())
    DelConnection(_profile->Uid());
    return 0;
}

int WebServer::NetThread::__OnReadEventTest(tcp::ConnectionProfile *_profile) {
    LogI("sleeping...")
    SOCKET fd = _profile->FD();
    std::this_thread::sleep_for(std::chrono::milliseconds(4000));
    char buff[1024] = {0, };
    
    while (true) {
        ssize_t n = ::read(fd, buff, 2);
        if (n == -1 && IS_EAGAIN(errno)) {
            LogI("EAGAIN")
            return 0;
        }
        if (n == 0) {
            LogI("Conn closed by peer")
            break;
        }
        if (n < 0) {
            LogE("err: n=%zd", n)
            break;
        }
        LogI("n: %zd", n)
        if (n > 0) {
            LogI("read: %s", buff)
        }
    }
    DelConnection(_profile->Uid());
    return 0;
}

void WebServer::NetThread::OnStart() {
    if (workers_.empty()) {
        LogE("call WebServer::SetWorker() to employ workers first!")
        assert(false);
    }
    // try launching all workers.
    for (auto & worker_thread : workers_) {
        worker_thread->Start();
    }
}

void WebServer::NetThread::__NotifyWorkersStop() {
    recv_queue_.Terminate();
    for (auto & worker_thread : workers_) {
        int seq = worker_thread->GetWorkerSeqNum();
        worker_thread->Join();
        delete worker_thread, worker_thread = nullptr;
        LogI("WorkerThread%d dead", seq)
    }
}

WebServer::NetThread::~NetThread() = default;

void WebServer::__NotifyNetThreadsStop() {
    for (auto & net_thread : net_threads_) {
        net_thread->NotifyStop();
        net_thread->Join();
        delete net_thread, net_thread = nullptr;
        LogI("NetThread dead")
    }
    LogI("All Threads Joined!")
}

bool WebServer::__IsNotifyStop(EpollNotifier::Notification &_notification) const {
    return _notification == notification_stop_;
}

int WebServer::__OnConnect() {
    SOCKET fd;
    struct sockaddr_in sock_in{};
    socklen_t socklen = sizeof(sockaddr_in);
    char ip_str[INET_ADDRSTRLEN] = {0, };
    uint16_t port = 0;
    
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
            LogE("errno(%d): %s", errno, strerror(errno));
            return -1;
        }
        if (!inet_ntop(AF_INET, &sock_in.sin_addr, ip_str, sizeof(ip_str))) {
            LogE("inet_ntop errno(%d): %s", errno, strerror(errno))
        }
        port = ntohs(sock_in.sin_port);
        std::string ip(ip_str);
        
        __AddConnection(fd, ip, port);
    }
}

void WebServer::__AddConnection(SOCKET _fd, std::string &_ip, uint16_t _port) {
    if (_fd < 0) {
        LogE("wtf??")
        return;
    }
    uint net_thread_idx = _fd % ServerConfig::net_thread_cnt;
    NetThread *owner_thread = net_threads_[net_thread_idx];
    if (!owner_thread) {
        LogE("wtf???")
        return;
    }
    owner_thread->AddConnection(_fd, _ip, _port);
    
}

int WebServer::__CreateListenFd() {
    assert(listenfd_.Create(AF_INET, SOCK_STREAM, 0) >= 0);
    
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

int WebServer::__OnEpollErr(int _fd) {
    LogE("fd: %d", _fd)
    return 0;
}

