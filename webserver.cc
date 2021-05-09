#include "webserver.h"
#include "utils/log.h"
#include "timeutil.h"
#include <cstring>


std::string WebServer::ServerConfig::field_max_backlog("max_backlog");
std::string WebServer::ServerConfig::field_worker_thread_cnt("worker_thread_cnt");
const char *const WebServer::kConfigFile = "webserverconf.yml";

WebServer::ServerConfig::ServerConfig()
        : ServerBase::ServerConfigBase()
        , max_backlog(0)
        , worker_thread_cnt(0) {
}


WebServer::WebServer() : HttpServer() {
}

void WebServer::AfterConfig() {
    SetNetThreadImpl<NetThread>();
    
    for (NetThreadBase *p : net_threads_) {
        auto *net_thread = (NetThread *) p;
        net_thread->SetMaxBacklog(((ServerConfig *) config_)->max_backlog);
    }
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


const size_t WebServer::NetThread::kDefaultMaxBacklog = 1024;

WebServer::NetThread::NetThread()
        : HttpNetThread()
        , max_backlog_(kDefaultMaxBacklog) {
    
}

bool WebServer::NetThread::IsWorkerOverload() { return recv_queue_.size() > max_backlog_ * 9 / 10; }

bool WebServer::NetThread::IsWorkerFullyLoad() { return recv_queue_.size() >= max_backlog_; }

size_t WebServer::NetThread::GetMaxBacklog() const { return max_backlog_; }

void WebServer::NetThread::SetMaxBacklog(size_t _backlog) { max_backlog_ = _backlog; }

void WebServer::NetThread::NotifySend() {
    epoll_notifier_.NotifyEpoll(notification_send_);
}

bool WebServer::NetThread::HandleNotification(EpollNotifier::Notification &_notification) {
    if (__IsNotifySend(_notification)) {
        HandleSend();
        return true;
    }
    return false;
}

void WebServer::NetThread::HandleSend() {
    tcp::SendContext *send_ctx;
    while (send_queue_.pop_front_to(send_ctx, false)) {
        LogD("fd(%d) doing send task", send_ctx->fd)
        bool is_send_done = _OnWriteEvent(send_ctx);
        if (is_send_done) {
            DelConnection(send_ctx->connection_uid);
        }
    }
}

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

void WebServer::NetThread::OnStop() {
    __NotifyWorkersStop();
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

bool WebServer::NetThread::__IsNotifySend(EpollNotifier::Notification &_notification) const {
    return _notification == notification_send_;
}

int WebServer::NetThread::HandleHttpPacket(tcp::ConnectionProfile *_conn) {
    if (_conn->GetType() != tcp::ConnectionProfile::kFrom) {
        LogE("NOT a http request, type: %d", _conn->GetType())
        return -1;
    }
    
    uint32_t uid = _conn->Uid();
    
    if (IsWorkerFullyLoad()) {
        LogI("worker fully loaded, drop connection directly")
        DelConnection(uid);
        return -1;
    }
    
    recv_queue_.push_back(_conn->GetRecvContext());
    return 0;
}

WebServer::NetThread::~NetThread() = default;


ServerBase::ServerConfigBase *WebServer::_MakeConfig() {
    return new ServerConfig();
}

bool WebServer::_CustomConfig(yaml::YamlDescriptor &_desc) {
    
    auto *config = (ServerConfig *) config_;
    
    if (yaml::Get(_desc, ServerConfig::field_max_backlog,
                  (int &) config->max_backlog) < 0) {
        LogE("Load max_backlog from yaml failed.")
        return false;
    }
    if (yaml::Get(_desc, ServerConfig::field_worker_thread_cnt,
                  (int &) config->worker_thread_cnt) < 0) {
        LogE("Load worker_thread_cnt from yaml failed.")
        return false;
    }
    if (config->worker_thread_cnt < 1) {
        LogE("Illegal worker_thread_cnt: %zu", config->worker_thread_cnt)
        return false;
    }
    if (config->worker_thread_cnt % config->net_thread_cnt != 0) {
        LogW("Config worker_thread_cnt as an integer multiple time of "
             "net_thread_cnt to get best performance.")
    }
    if (config->worker_thread_cnt > 4 * config->net_thread_cnt) {
        LogW("Excessive proportion of worker_thread / net_thread "
             "may lower performance of net_thread.")
    }
    if (config->max_backlog < 0) {
        LogE("Please config max_backlog a positive number.")
        return false;
    }
    return true;
}

const char *WebServer::ConfigFile() {
    return kConfigFile;
}

