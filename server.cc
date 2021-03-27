#include "server.h"
#include "utils/log.h"
#include "socketepoll.h"
#include "http/httprequest.h"
#include "yamlutil.h"
#include "signalhandler.h"
#include "timeutil.h"
#include <unistd.h>
#include <iostream>
#include <string.h>
#include <execinfo.h>


std::string Server::ServerConfig::field_port("port");

uint16_t Server::ServerConfig::port = 0;

std::string Server::ServerConfig::field_net_thread_cnt("net_thread_cnt");

size_t Server::ServerConfig::net_thread_cnt = 0;

bool Server::ServerConfig::is_config_done = false;


Server::Server()
        : listenfd_(-1)
        , net_thread_cnt_(1)
        , running_(false) {
    
    Yaml::YamlDescriptor server_config = Yaml::Load("../framework/serverconfig.yaml");
    
    if (!server_config) {
        LogE(__FILE__, "[Server] serverconfig.yaml open failed!")
        return;
    }
    
    do {
        if (Yaml::GetUShort(server_config, ServerConfig::field_port,
                      ServerConfig::port) < 0) {
            LogE(__FILE__, "[Server] load port from yaml failed")
            break;
        }
        if (Yaml::GetInt(server_config, ServerConfig::field_net_thread_cnt,
                         (int &) ServerConfig::net_thread_cnt) < 0) {
            LogE(__FILE__, "[Server] load net_thread_cnt from yaml failed")
            break;
        }
        
        net_thread_cnt_ = ServerConfig::net_thread_cnt;
        
        if (__CreateListenFd() < 0) { return; }
        
        if (__Bind(ServerConfig::port) < 0) { return; }
        
        for (int i = 0; i < net_thread_cnt_; ++i) {
            auto net_thread = new NetThread();
            net_threads_.push_back(net_thread);
        }
        
        ServerConfig::is_config_done = true;
    
        SignalHandler::Instance().Init();
        
    } while (false);
    
    Yaml::Close(server_config);
    
}


void Server::Serve() {
    if (!ServerConfig::is_config_done) {
        LogE(__FILE__, "[Serve] config me first!")
        return;
    }
    if (worker_threads_.empty()) {
        LogE(__FILE__, "[Serve] employ workers first!")
        return;
    }
    
    running_ = true;
    
    ::listen(listenfd_, 1024);
    
    socket_epoll_.SetListenFd(listenfd_);
    
    for (auto &net_thread : net_threads_) {
        net_thread->Start();
    }
    
    for (auto &worker_thread : worker_threads_) {
        worker_thread->Start();
    }
    
    while (running_) {
        int n_events = socket_epoll_.EpollWait();
        
        if (n_events < 0) {
            if (socket_epoll_.GetErrNo() == EINTR) {
                continue;
            }
            running_ = false;
        }
        
        for (int i = 0; i < n_events; i++) {
            if (socket_epoll_.IsNewConnect(i)) {
                __OnConnect();
                
            } else if (SOCKET fd = socket_epoll_.IsErrSet(i)) {
                __HandleErr(fd);
            }
        }
    }
}

Server::~Server() {
    running_ = false;
    if (listenfd_ != -1) {
        LogI(__FILE__, "[~Server] close listenfd")
        ::close(listenfd_);
    }
    for (auto & net_thread : net_threads_) {
        net_thread->Stop();
        delete net_thread, net_thread = nullptr;
        LogI(__FILE__, "NetThread dead")
    }
    
    for (auto & worker_thread : worker_threads_) {
        worker_thread->Stop();
        delete worker_thread, worker_thread = nullptr;
        LogI(__FILE__, "WorkerThread dead")
    }
}


Server::WorkerThread::WorkerThread()
        : net_thread_(nullptr) {
    
}

void Server::WorkerThread::Run() {
    if (!net_thread_) {
        LogE(__FILE__, "[WorkerThread::Run] bind NetThread first!")
        running_ = false;
        return;
    }
    LogI(__FILE__, "[:NetThread::Run] launching WorkerThread!")
    
    auto recv_queue = net_thread_->GetRecvQueue();
    
    while (running_) {
        
        Tcp::RecvContext *recv_ctx;
        
        if (recv_queue->pop_front(recv_ctx)) {
            HandleImpl(recv_ctx);
        }
    }
}

void Server::WorkerThread::BindNetThread(Server::NetThread *_net_thread) {
    if (_net_thread) {
        net_thread_ = _net_thread;
    }
}

void Server::WorkerThread::Stop() {
    running_ = false;
}

Server::WorkerThread::~WorkerThread() {

}

Server::EpollNotifier::EpollNotifier()
        : fd_(INVALID_SOCKET)
        , socket_epoll_(nullptr) {
    
    fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0) {
        LogE(__FILE__, "[EpollNotifier] create socket error: %s, errno: %d",
             strerror(errno), errno);
        return;
    }
    LogI(__FILE__, "[EpollNotifier] notify fd: %d", fd_)
    SetNonblocking(fd_);
}

void Server::EpollNotifier::SetSocketEpoll(SocketEpoll *_epoll) {
    if (socket_epoll_) {
        LogE(__FILE__, "[SetSocketEpoll] socket_epoll_ already set!")
        return;
    }
    if (_epoll) {
        socket_epoll_ = _epoll;
        socket_epoll_->AddSocketRead(fd_);
    }
}

void Server::EpollNotifier::NotifyEpoll() {
    if (socket_epoll_) {
        void *epoll_data = (void *) (uint64_t) fd_;
        socket_epoll_->ModSocketWrite(fd_, epoll_data);
    }
}

SOCKET Server::EpollNotifier::GetNotifyFd() const { return fd_; }

Server::EpollNotifier::~EpollNotifier() {
    if (fd_ != INVALID_SOCKET) {
        if (socket_epoll_) {
            socket_epoll_->DelSocket(fd_);
        }
        ::close(fd_);
        fd_ = INVALID_SOCKET;
    }
}


Server::ConnectionManager::ConnectionManager()
        : socket_epoll_(nullptr) {
}

void Server::ConnectionManager::SetEpoll(SocketEpoll *_epoll) {
    if (_epoll) {
        socket_epoll_ = _epoll;
    }
}

Tcp::ConnectionProfile *Server::ConnectionManager::GetConnection(SOCKET _fd) {
    ScopedLock lock(mutex_);
    auto find = connections_.find(_fd);
    if (find == connections_.end()) {
        return nullptr;
    }
    return find->second;
}

void Server::ConnectionManager::AddConnection(SOCKET _fd,
                                              Tcp::ConnectionProfile *_conn) {
    ScopedLock lock(mutex_);
    if (connections_.find(_fd) != connections_.end()) {
        LogE(__FILE__, "[ConnectionManager::AddConnection] already got %d", _fd)
        return;
    }
    if (!_conn || _fd < 0) {
        return;
    }
    connections_[_fd] = _conn;
    if (socket_epoll_) {
        // While one thread is blocked in a call to epoll_wait(), it is
        // possible for another thread to add a file descriptor to the
        // waited-upon epoll instance.  If the new file descriptor becomes
        // ready, it will cause the epoll_wait() call to unblock.
        socket_epoll_->AddSocketRead(_fd);
    }
}

void Server::ConnectionManager::DelConnection(SOCKET _fd) {
    ScopedLock lock(mutex_);
    auto conn = connections_[_fd];
    if (socket_epoll_) {
        socket_epoll_->DelSocket(_fd);
    }
    if (conn) {
        conn->CloseSelf();
    } else {
        LogW(__FILE__, "[DelConnection] Bug here!!")
    }
    connections_.erase(_fd);
    delete conn, conn = nullptr;
    
}

void Server::ConnectionManager::ClearTimeout() {
    uint64_t now = ::gettickcount();
    ScopedLock lock(mutex_);
    for (auto &it : connections_) {
        if (it.second->IsTimeout(now)) {
            LogI(__FILE__, "[ClearTimeout] clear timeout fd: %d", it.first)
            DelConnection(it.first);
        }
    }
}

Server::ConnectionManager::~ConnectionManager() {
    ScopedLock lock(mutex_);
    for (auto &it : connections_) {
        delete it.second, it.second = nullptr;
    }
}


Server::NetThread::NetThread()
        : Thread() {
    connection_manager_.SetEpoll(&socket_epoll_);
    notifier_.SetSocketEpoll(&socket_epoll_);
}

void Server::NetThread::Run() {
    LogI(__FILE__, "[NetThread::Run] launching NetThread!")
    running_ = true;
    int epoll_retry = 3;
    
    while (running_) {
        
        int n_events = socket_epoll_.EpollWait(3000);
        
        if (n_events < 0) {
            if (socket_epoll_.GetErrNo() == EINTR) {
                continue;
            }
            if (--epoll_retry > 0) {
                continue;
            }
            running_ = false;
            break;
        }
    
        static uint64_t last_clear_ts = 0;
        uint64_t now = ::gettickcount();
        if (now - last_clear_ts > 3000) {
            last_clear_ts = now;
            ClearTimeout();
        }
    
        for (int i = 0; i < n_events; ++i) {
            if (socket_epoll_.GetSocket(i) == notifier_.GetNotifyFd()) {
                HandleSend();
                continue;
            }
            
            if (SOCKET fd = socket_epoll_.IsReadSet(i)) {
                __OnReadEvent(fd);

            } else if (void *ptr = socket_epoll_.IsWriteSet(i)) {
                auto send_ctx = (Tcp::SendContext *) ptr;
                __OnWriteEvent(send_ctx, false);

            } else if ((fd = socket_epoll_.IsErrSet(i))) {
                __OnErrEvent(fd);
                
            } else {
                LogE(__FILE__, "[NetThread::Run] did Not process this event!")
            }
        }
    }
    
}

void Server::NetThread::NotifyEpoll() { notifier_.NotifyEpoll(); }

void Server::NetThread::AddConnection(int _fd) {
    if (_fd < 0) {
        LogE(__FILE__, "[AddConnection] invalid fd: %d", _fd)
        return;
    }
    auto neo = new Tcp::ConnectionProfile(_fd);
    connection_manager_.AddConnection(_fd, neo);
}

void Server::NetThread::DelConnection(int _fd) {
    if (_fd < 0) {
        LogE(__FILE__, "[AddConnection] invalid fd: %d", _fd)
        return;
    }
    connection_manager_.DelConnection(_fd);
}

void Server::NetThread::HandleSend() {
    Tcp::SendContext *send_ctx;
    while (send_queue_.pop_front(send_ctx, false)) {
        LogI(__FILE__, "[NetThread::HandleSend] handle")
        __OnWriteEvent(send_ctx, true);
    }
}

void Server::NetThread::ClearTimeout() { connection_manager_.ClearTimeout(); }

Server::RecvQueue *Server::NetThread::GetRecvQueue() { return &recv_queue_; }

Server::SendQueue *Server::NetThread::GetSendQueue() { return &send_queue_; }

void Server::NetThread::Stop() {
    // FIXME lock
    running_ = false;
}

void Server::NetThread::HandleException(std::exception &ex) {
    Thread::HandleException(ex);
    LogE(__FILE__, "[HandleException] %s", ex.what())
}

int Server::NetThread::__OnReadEvent(SOCKET _fd) {
    if (_fd <= 0) {
        LogE(__FILE__, "[__OnReadEvent] invalid _fd: %d", _fd)
        return -1;
    }
    LogI(__FILE__, "[__OnReadEvent] fd: %d", _fd)
    
    Tcp::ConnectionProfile *conn = connection_manager_.GetConnection(_fd);
    if (!conn) {
        LogE(__FILE__, "[__OnReadEvent] conn == NULL")
        return -1;
    }
    
    int ret = conn->Receive();
    
    if (ret < 0) {
        DelConnection(_fd);
        return -1;
    }
    
    if (conn->IsParseDone()) {
        LogI(__FILE__, "[__OnReadEvent] http parse succeed")
        recv_queue_.push(conn->GetRecvContext());
    }
    return 0;
}

int Server::NetThread::__OnWriteEvent(Tcp::SendContext *_send_ctx, bool _mod_write) {
    if (!_send_ctx) {
        LogE(__FILE__, "[__OnWriteEvent] !_send_ctx")
        return -1;
    }
    AutoBuffer &resp = _send_ctx->buffer;
    size_t pos = resp.Pos();
    size_t ntotal = resp.Length() - pos;
    SOCKET fd = _send_ctx->fd;
    
    ssize_t nsend = ::write(fd, resp.Ptr(pos), ntotal);
    
    do {
        if (nsend == ntotal) {
            LogI(__FILE__, "[__OnWriteEvent] send %zd/%zu B without epoll", nsend, ntotal)
            break;
        }
        if (nsend >= 0 || (nsend < 0 && IS_EAGAIN(errno))) {
            nsend = nsend > 0 ? nsend : 0;
            LogI(__FILE__, "[__OnWriteEvent] fd(%d): send %zd/%zu B", fd, nsend, ntotal)
            resp.Seek(pos + nsend);
            if (_mod_write) {
                socket_epoll_.ModSocketWrite(fd, (void *)_send_ctx);
            }
            return 0;
        }
        if (nsend < 0) {
            LogE(__FILE__, "[__OnWriteEvent] nsend(%zu), errno(%d): %s",
                 nsend, errno, strerror(errno));
        }
    } while (false);
    
    connection_manager_.DelConnection(fd);
    return nsend < 0 ? -1 : 0;
}

int Server::NetThread::__OnErrEvent(int _fd) {
    LogE(__FILE__, "[__OnErrEvent] fd: %d", _fd)
    return 0;
}

int Server::NetThread::__OnReadEventTest(SOCKET _fd) {
    LogI(__FILE__, "[__HandleReadTest] sleeping...")
    sleep(4);
    char buff[1024] = {0, };
    
    while (true) {
        ssize_t n = ::read(_fd, buff, 2);
        if (n == -1 && IS_EAGAIN(errno)) {
            LogI(__FILE__, "[__HandleReadTest] EAGAIN")
            return 0;
        }
        if (n == 0) {
            LogI(__FILE__, "[__HandleReadTest] Conn closed by peer")
            break;
        }
        if (n < 0) {
            LogE(__FILE__, "[__HandleReadTest] err: n=%zd", n)
            break;
        }
        LogI(__FILE__, "[__HandleReadTest] n: %zd", n)
        if (n > 0) {
            LogI(__FILE__, "read: %s", buff)
        }
    }
    socket_epoll_.DelSocket(_fd);
    ::shutdown(_fd, SHUT_RDWR);
    return 0;
}

Server::NetThread::~NetThread() {

}

int Server::__OnConnect() {
    LogI(__FILE__, "[__HandleConnect] IsNewConnect")
    SOCKET fd;
    while (true) {
        fd = ::accept(listenfd_, (struct sockaddr *) NULL, NULL);
        if (fd < 0) {
            if (IS_EAGAIN(errno)) { return 0; }
            LogE(__FILE__, "[__HandleConnect] errno(%d): %s",
                 errno, strerror(errno));
            return -1;
        }
        SetNonblocking(fd);
        LogI(__FILE__, "[__HandleConnect] new connect, fd: %d", fd);
        __AddConnection(fd);
    }
}

void Server::__AddConnection(SOCKET _fd) {
    if (_fd < 0) {
        return;
    }
    NetThread *owner_thread = net_threads_[_fd % net_thread_cnt_];
    if (!owner_thread) {
        LogE(__FILE__, "[__AddConnection] wtf???")
        return;
    }
    owner_thread->AddConnection(_fd);
    
}

int Server::__CreateListenFd() {
    listenfd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd_ < 0) {
        LogE(__FILE__, "[__CreateListenFd] create socket error: %s, errno: %d",
             strerror(errno), errno);
        return -1;
    }
    // FIXME
    struct linger ling;
    ling.l_linger = 0;
    ling.l_onoff = 1;
    ::setsockopt(listenfd_, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling));
    SetNonblocking(listenfd_);
    return 0;
}


int Server::__Bind(uint16_t _port) {
    struct sockaddr_in sock_addr;
    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    sock_addr.sin_port = htons(_port);
    
    int ret = ::bind(listenfd_, (struct sockaddr *) &sock_addr,
                        sizeof(sock_addr));
    if (ret < 0) {
        LogE(__FILE__, "[__Bind] errno(%d): %s", errno, strerror(errno));
        return -1;
    }
    return 0;
}

int Server::__HandleErr(int _fd) {
    LogE(__FILE__, "[__HandleErr] fd: %d", _fd)
    return 0;
}

