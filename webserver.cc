#include "webserver.h"
#include "utils/log.h"
#include "socketepoll.h"
#include "http/httprequest.h"
#include "yamlutil.h"
#include "signalhandler.h"
#include "timeutil.h"
#include <unistd.h>
#include <string.h>
#include <cassert>


std::string WebServer::ServerConfig::field_port("port");

uint16_t WebServer::ServerConfig::port = 0;

std::string WebServer::ServerConfig::field_net_thread_cnt("net_thread_cnt");

size_t WebServer::ServerConfig::net_thread_cnt = 0;

bool WebServer::ServerConfig::is_config_done = false;


WebServer::WebServer()
        : listenfd_(-1)
        , net_thread_cnt_(1)
        , running_(false) {
    
    yaml::YamlDescriptor server_config = yaml::Load("../framework/serverconfig.yml");
    
    if (!server_config) {
        LogE("serverconfig.yaml open failed!")
        return;
    }
    
    do {
        if (yaml::Get(server_config, ServerConfig::field_port,
                      ServerConfig::port) < 0) {
            LogE("load port from yaml failed")
            break;
        }
        if (yaml::Get(server_config, ServerConfig::field_net_thread_cnt,
                         (int &) ServerConfig::net_thread_cnt) < 0) {
            LogE("load net_thread_cnt from yaml failed")
            break;
        }
        if (ServerConfig::net_thread_cnt < 1) {
            ServerConfig::net_thread_cnt = 1;
        }
        if (ServerConfig::net_thread_cnt > 8) {
            ServerConfig::net_thread_cnt = 8;
        }
        net_thread_cnt_ = ServerConfig::net_thread_cnt;
        
        if (__CreateListenFd() < 0) { break; }
        
        if (__Bind(ServerConfig::port) < 0) { break; }
        
        for (int i = 0; i < net_thread_cnt_; ++i) {
            auto net_thread = new NetThread();
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
    
    ::listen(listenfd_, 1024);
    
    socket_epoll_.SetListenFd(listenfd_);
    
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
    
    if (listenfd_ != -1) {
        LogI("close listenfd")
        ::close(listenfd_), listenfd_ = -1;
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


WebServer::ConnectionManager::ConnectionManager()
        : socket_epoll_(nullptr) {
}

void WebServer::ConnectionManager::SetEpoll(SocketEpoll *_epoll) {
    if (_epoll) {
        socket_epoll_ = _epoll;
    }
}

tcp::ConnectionProfile *WebServer::ConnectionManager::GetConnection(SOCKET _fd) {
    ScopedLock lock(mutex_);
    auto find = connections_.find(_fd);
    if (find == connections_.end()) {
        return nullptr;
    }
    return find->second;
}

void WebServer::ConnectionManager::AddConnection(SOCKET _fd, std::string &_ip, uint16_t _port) {
    if (_fd < 0) {
        LogE("%d", _fd)
        LogPrintStacktrace()
        return;
    }
    ScopedLock lock(mutex_);
    if (connections_.find(_fd) != connections_.end()) {
        LogE("already got %d", _fd)
        return;
    }
    auto neo = new tcp::ConnectionProfile(_fd, _ip, _port);
    connections_.emplace(_fd, neo);
    if (socket_epoll_) {
        // While one thread is blocked in a call to epoll_wait(), it is
        // possible for another thread to add a file descriptor to the
        // waited-upon epoll instance.  If the new file descriptor becomes
        // ready, it will cause the epoll_wait() call to unblock.
        socket_epoll_->AddSocketReadWrite(_fd, (uint64_t) neo);
    }
}

void WebServer::ConnectionManager::DelConnection(SOCKET _fd) {
    ScopedLock lock(mutex_);
    auto &conn = connections_[_fd];
    if (socket_epoll_) {
        socket_epoll_->DelSocket(_fd);
    }
    if (conn) {
        LogI("fd(%d)", _fd)
    } else {
        LogE("fd(%d) Bug here!!", _fd)
    }
    delete conn, conn = nullptr;
    connections_.erase(_fd);
    
}

void WebServer::ConnectionManager::ClearTimeout() {
    uint64_t now = ::gettickcount();
    ScopedLock lock(mutex_);
    auto it = connections_.begin();
    while (it != connections_.end()) {
        if (it->second->IsTimeout(now)) {
            LogI("clear fd: %d", it->first)
            socket_epoll_->DelSocket(it->first);
            delete it->second, it->second = nullptr;
            it = connections_.erase(it);
            continue;
        }
        ++it;
    }
}

WebServer::ConnectionManager::~ConnectionManager() {
    ScopedLock lock(mutex_);
    for (auto &it : connections_) {
        delete it.second, it.second = nullptr;
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
    LogI("launching NetThread!")
    int epoll_retry = 3;
    
    const uint64_t clear_timeout_period = 3000;
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
                SOCKET fd = profile->FD();
                __OnReadEvent(fd);
            }
            if ((profile = (tcp::ConnectionProfile *) socket_epoll_.IsWriteSet(i))) {
                auto send_ctx = profile->GetSendContext();
                __OnWriteEvent(send_ctx);
            }
            if ((profile = (tcp::ConnectionProfile *) socket_epoll_.IsErrSet(i))) {
                SOCKET fd = profile->FD();
                __OnErrEvent(fd);
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

void WebServer::NetThread::DelConnection(SOCKET _fd) {
    if (_fd < 0) {
        LogE("invalid fd: %d", _fd)
        return;
    }
    connection_manager_.DelConnection(_fd);
}

void WebServer::NetThread::HandleSend() {
    tcp::SendContext *send_ctx;
    while (send_queue_.pop_front_to(send_ctx, false)) {
        LogI("fd(%d) doing send task", send_ctx->fd)
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

int WebServer::NetThread::__OnReadEvent(SOCKET _fd) {
    if (_fd <= 0) {
        LogE("invalid _fd: %d", _fd)
        return -1;
    }
    LogI("fd: %d", _fd)
    
    tcp::ConnectionProfile *conn = connection_manager_.GetConnection(_fd);
    if (!conn) {
        LogE("conn == NULL, fd: %d", _fd)
        return -1;
    }
    
    if (conn->Receive() < 0) {
        DelConnection(_fd);
        return -1;
    }
    
    if (conn->IsParseDone()) {
        LogI("fd(%d) http parse succeed", _fd)
        if (IsWorkerFullyLoad()) {
            LogI("worker fully loaded, drop connection directly")
            DelConnection(_fd);
            
        } else {
            recv_queue_.push_back(conn->GetRecvContext());
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
            LogI("send %zd/%zu B, done", nsend, ntotal)
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
    
    DelConnection(fd);
    
    return nsend < 0 ? -1 : 0;
}

int WebServer::NetThread::__OnErrEvent(int _fd) {
    LogE("fd: %d", _fd)
    DelConnection(_fd);
    return 0;
}

int WebServer::NetThread::__OnReadEventTest(SOCKET _fd) {
    LogI("sleeping...")
    std::this_thread::sleep_for(std::chrono::milliseconds(4000));
    char buff[1024] = {0, };
    
    while (true) {
        ssize_t n = ::read(_fd, buff, 2);
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
    DelConnection(_fd);
    return 0;
}

void WebServer::NetThread::OnStarted() {
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
    
    while (true) {
        fd = ::accept(listenfd_, (struct sockaddr *) &sock_in, &socklen);
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
        LogI("fd(%d), address: [%s:%d]", fd, ip_str, port);
        SetNonblocking(fd);
        __AddConnection(fd, ip, port);
    }
}

void WebServer::__AddConnection(SOCKET _fd, std::string &_ip, uint16_t _port) {
    if (_fd < 0) {
        return;
    }
    uint net_thread_idx = _fd % net_thread_cnt_;
    NetThread *owner_thread = net_threads_[net_thread_idx];
    if (!owner_thread) {
        LogE("wtf???")
        return;
    }
    owner_thread->AddConnection(_fd, _ip, _port);
    
}

int WebServer::__CreateListenFd() {
    listenfd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd_ < 0) {
        LogE("create socket error: %s, errno: %d",
             strerror(errno), errno);
        return -1;
    }
    // FIXME
    struct linger ling{};
    ling.l_linger = 0;
    ling.l_onoff = 1;
    ::setsockopt(listenfd_, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling));
    SetNonblocking(listenfd_);
    return 0;
}


int WebServer::__Bind(uint16_t _port) const {
    struct sockaddr_in sock_addr{};
    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    sock_addr.sin_port = htons(_port);
    
    int ret = ::bind(listenfd_, (struct sockaddr *) &sock_addr,
                        sizeof(sock_addr));
    if (ret < 0) {
        LogE("errno(%d): %s", errno, strerror(errno));
        return -1;
    }
    return 0;
}

int WebServer::__OnEpollErr(int _fd) {
    LogE("fd: %d", _fd)
    return 0;
}

