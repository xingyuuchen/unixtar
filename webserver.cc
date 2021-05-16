#include "webserver.h"
#include "utils/log.h"
#include "timeutil.h"
#include "http/httprequest.h"
#include "netscenesvrheartbeat.pb.h"
#include <cstring>


const char *const WebServer::ServerConfig::key_max_backlog("max_backlog");
const char *const WebServer::ServerConfig::key_worker_thread_cnt("worker_thread_cnt");
const char *const WebServer::ServerConfig::key_reverse_proxy("reverse_proxy");
const char *const WebServer::ServerConfig::key_ip("ip");
const char *const WebServer::ServerConfig::key_is_send_heartbeat("send_heartbeat");
const char *const WebServer::ServerConfig::key_heartbeat_period("heartbeat_period");
const char *const WebServer::kConfigFile = "webserverconf.yml";
const int WebServer::kDefaultHeartBeatPeriod = 60;

WebServer::ServerConfig::ServerConfig()
        : ServerBase::ServerConfigBase()
        , max_backlog(0)
        , worker_thread_cnt(0)
        , reverse_proxy_port(0)
        , is_send_heartbeat(false)
        , heartbeat_period(kDefaultHeartBeatPeriod) {
}


WebServer::WebServer() : ServerBase() {
}

void WebServer::AfterConfig() {
    SetNetThreadImpl<NetThread>();
    
    for (NetThreadBase *p : net_threads_) {
        auto *net_thread = (NetThread *) p;
        net_thread->SetMaxBacklog(((ServerConfig *) config_)->max_backlog);
    }
}

void WebServer::SendHeartbeat() {
    auto *config = (ServerConfig *) config_;
    if (!config->is_config_done) {
        LogE("config not done, give up sending heartbeat")
        return;
    }
    NetSceneSvrHeartbeatProto::NetSceneSvrHeartbeatReq req;
    uint32_t backlog = 0;
    for (auto & net_thread : net_threads_) {
        backlog += ((NetThread *) net_thread)->Backlog();
    }
    req.set_request_backlog(backlog);
    req.set_port(config->port);
    std::string ba = req.SerializeAsString();
    
    auto *net_thread = ((NetThread *) net_threads_[0]);
    auto conn = net_thread->MakeConnection(config->reverse_proxy_ip,
                                           config->reverse_proxy_port);
    if (!conn) {
        LogE("reverse proxy down, give up sending heartbeat")
        return;
    }
    conn->MakeSendContext();
    http::request::Pack(config->reverse_proxy_ip, "/",
                        nullptr, ba,
                        conn->GetSendContext()->buffer);
    if (NetThread::_OnWriteEvent(conn->GetSendContext())) {
        net_thread->DelConnection(conn->Uid());
    }
    LogD("pit pat")
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
        
        tcp::RecvContext *recv_ctx;
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
        : NetThreadBase()
        , max_backlog_(kDefaultMaxBacklog) {
    
}

bool WebServer::NetThread::IsWorkerOverload() { return recv_queue_.size() > max_backlog_ * 9 / 10; }

bool WebServer::NetThread::IsWorkerFullyLoad() { return recv_queue_.size() >= max_backlog_; }

size_t WebServer::NetThread::GetMaxBacklog() const { return max_backlog_; }

size_t WebServer::NetThread::Backlog() { return recv_queue_.size(); }

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

void WebServer::NetThread::ConfigApplicationLayer(tcp::ConnectionProfile *_conn) {
    if (_conn->GetType() != tcp::ConnectionProfile::kFrom) {
        return;     // heartbeat packet, pass.
    }
    _conn->ConfigApplicationLayer<http::request::HttpRequest,
                                  http::request::Parser>();
}

int WebServer::NetThread::HandleApplicationPacket(tcp::ConnectionProfile *_conn) {
    assert(_conn);
    uint32_t uid;
    do {
        uid = _conn->Uid();
        if (_conn->ApplicationProtocol() != kHttp1_1) {
            LogE("%s is NOT a http1.1 packet", _conn->ApplicationProtocolName())
            break;
        }
        if (_conn->GetType() != tcp::ConnectionProfile::kFrom) {
            LogE("NOT a http request, type: %d", _conn->GetType())
            break;
        }
        if (IsWorkerFullyLoad()) {
            LogI("worker fully loaded, drop connection directly")
            break;
        }
        recv_queue_.push_back(_conn->GetRecvContext());
        return 0;
        
    } while (false);
    
    DelConnection(uid);
    return -1;
}

WebServer::NetThread::~NetThread() = default;


ServerBase::ServerConfigBase *WebServer::_MakeConfig() {
    return new ServerConfig();
}

bool WebServer::_CustomConfig(yaml::YamlDescriptor *_desc) {
    
    auto *config = (ServerConfig *) config_;
    
    yaml::ValueLeaf *max_backlog = _desc->GetLeaf(ServerConfig::key_max_backlog);
    if (!max_backlog) {
        LogE("Load max_backlog from yaml failed.")
        return false;
    }
    max_backlog->To((int &) config->max_backlog);

    yaml::ValueLeaf *worker_cnt = _desc->GetLeaf(ServerConfig::key_worker_thread_cnt);
    if (!worker_cnt) {
        LogE("Load worker_thread_cnt from yaml failed.")
        return false;
    }
    worker_cnt->To((int &) config->worker_thread_cnt);
    
    yaml::ValueObj *reverse_proxy = _desc->GetYmlObj(ServerConfig::key_reverse_proxy);
    if (!reverse_proxy) {
        LogE("Load reverse_proxy from yaml failed.")
        return false;
    }
    reverse_proxy->GetLeaf(ServerConfig::key_ip)->To(config->reverse_proxy_ip);
    reverse_proxy->GetLeaf(ServerConfig::key_port)->To(config->reverse_proxy_port);
    reverse_proxy->GetLeaf(ServerConfig::key_is_send_heartbeat)->To(config->is_send_heartbeat);
    reverse_proxy->GetLeaf(ServerConfig::key_heartbeat_period)->To(config->heartbeat_period);
    
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
        config->worker_thread_cnt = 4 * config->net_thread_cnt;
    }
    if (config->max_backlog < 0) {
        LogE("Please config max_backlog a positive number.")
        return false;
    }
    if (config->heartbeat_period < 30) {
        LogE("Please config Heartbeat period greater than 30 seconds.")
        return false;
    }
    config->heartbeat_period *= 1000;   // ms => s
    
    LogI("port: %d, net_thread_cnt: %zu, worker_thread_cnt: %zu, max_backlog: %zu, "
         "reverse_proxy: [%s:%d], send_heart_beat: %d, heartbeat_period: %d",
         config->port, config->net_thread_cnt, config->worker_thread_cnt,
         config->max_backlog, config->reverse_proxy_ip.c_str(), config->reverse_proxy_port,
         config->is_send_heartbeat, config->heartbeat_period)
    return true;
}

const char *WebServer::ConfigFile() {
    return kConfigFile;
}

void WebServer::LoopingEpollWait() {
    if (((ServerConfig *) config_)->is_send_heartbeat) {
        SendHeartbeat();
    }
}

int WebServer::EpollLoopInterval() {
    assert(config_->is_config_done);
    auto *config = (ServerConfig *) config_;
    if (config->is_send_heartbeat) {
        return ((ServerConfig *) config_)->heartbeat_period;
    }
    return ServerBase::EpollLoopInterval();
}

