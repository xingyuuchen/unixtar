# 🌟 Unixtar

### `✨Unique Star✨`

[English version portal](README.md)

一个不知道自己性能高不高的服务端框架。

不包含任何第三方库，框架从低层级的系统调用和C标准库写起。

框架采用 `Epoll + NetThreads + WorkerThreads` 的模型。

特点：
* 传输层和IP层直接使用unix域套接字，每个网络线程配合一个epoll对象和多个工作线程以提供并发能力。并提供过载保护。
* Http服务器。负责短连接请求。框架已完成Http协议Serialize与Parse。请求包体可用`Protobuf`序列化数据（业务代码请继承自`NetSceneProtoBuf`）。
* WebSocket服务器。负责长连接请求，提供向自身或（和）其他连接主动推送消息的能力。框架已完成WebSocket协议握手、Serialize、Parse、挥手。
* 反向代理。提供转发请求到服务节点、负载均衡的能力。你可以从多个负载均衡策略中选择。
* 传输层模块的代码完全独立于具体的应用层协议，你可以通过继承 `ApplicationPacket` 类，轻易地添加自己的应用层协议。
* 框架完全独立于业务，你可以通过继承 `NetSceneBase` 类，轻易地添加自己的网络接口。


## ✨ 编译 (unix)
```shell
git clone --recursive https://github.com/xingyuuchen/unixtar.git framework
cd framework/script
bash autogen.sh    # 当你需要使用protobuf时运行此脚本
bash cmake.sh -d   # -d选项将使得进程成为linux的守护进程。日志通过rsyslog系统重定向到制定的文件
```

## ✨ 用法
### Http 短链接请求
你仅仅需要写 `NetScene` 来完成你的业务逻辑。

每个网络接口采用一个类（`NetScene_xxx`）表示，他们直接或间接地继承自 `NetSceneBase` ，你可以把它当作Java中的 `Servlet` 便于理解。

如果你使用 `ProtoBuf` 来序列化数据，请继承自 `NetSceneProtoBuf` ，并使用 `POST` 方法。

否则，如果你想定制自己的数据序列化方式，请继承自 `NetSceneCustom` ，然后使用 `GET` 或者 `POST`。

定义好网络接口并实现完自己的业务逻辑后，请向框架注册它：
`NetSceneDispatcher::Instance()::RegisterNetScene<NetScene_YourBusiness>();`

另外，你可以通过修改 `webserverconf.yml` 来定制一些服务端配置。你可以定制：
* 进程监听的端口。
* 处理网络事件的线程数。
* 处理业务逻辑的线程数。
* 最大的连接数。
* 最大的业务积压量（超过此阈值被认为过载）。
* 反向代理服务器信息、发送心跳包的周期。


之后，只用在 `main` 函数中调用 `WebServer::Instance().Serve();` ，你就能愉快地开启服务了。


```c++
#include "log.h"
#include "webserver.h"
#include "netscenedispatcher.h"
#ifdef DAEMON
#include "daemon.h"
#endif

int main(int ac, char **argv) {
#ifdef DAEMON
    if (unixtar::Daemonize() < 0) {
        printf("daemonize failed\n");
        return 0;
    }
#endif
    
    logger::OpenLog(argv[0]);

    WebServer::Instance().Config();
    
    LogI("Launching Server...")
    
    // NetScene_YourBusiness must inherit from NetSceneBase, which is your
    // predefined network interface (e.g. A specific Http url route)
    // See class NetSceneGetIndexPage below for detail.
    NetSceneDispatcher::Instance().RegisterNetScene<NetScene_YourBusiness>();
    NetSceneDispatcher::Instance().RegisterNetScene<NetScene_YourBusiness1>();
    NetSceneDispatcher::Instance().RegisterNetScene<NetScene_YourBusiness2>();
    
    WebServer::Instance().Serve();
    
    LogI("Webserver Down")
    return 0;
}
```

#### 这是一个网络接口类的示例（用于访问主页）:
```c++
#pragma once
#include "netscenecustom.h"
#include <mutex>

class NetSceneGetIndexPage : public NetSceneCustom {
  public:
    NetSceneGetIndexPage();

    // 全局唯一的Type，标定网络接口。
    int GetType() override;

    // 返回一个新的本类对象。
    NetSceneBase *NewInstance() override;

    // 你的业务逻辑实现于此。
    int DoSceneImpl(const std::string &_in_buffer) override;
    
    // Http body 首地址。
    void *Data() override;

    // Http body 长度。
    size_t Length() override;

    // Http url 路由。
    const char *Route() override;

    // Http Content-Type.
    const char *ContentType() override;

  private:
    char                        resp_[128] {0, };
    static const char *const    kUrlRoute;
    static const char *const    kRespFormat;
    static std::mutex           mutex_;
    
};
```
```c++
#include "netscene_getindexpage.h"
#include "constantsprotocol.h"
#include "log.h"
#include "http/headerfield.h"

const char *const NetSceneGetIndexPage::kUrlRoute = "/";

const char *const NetSceneGetIndexPage::kRespFormat =
        "If you see this, the server is running normally, %d visits since last boot.";

std::mutex NetSceneGetIndexPage::mutex_;

NetSceneGetIndexPage::NetSceneGetIndexPage() : NetSceneCustom() {}

int NetSceneGetIndexPage::GetType() { return 0; }

NetSceneBase *NetSceneGetIndexPage::NewInstance() { return new NetSceneGetIndexPage(); }

int NetSceneGetIndexPage::DoSceneImpl(const std::string &_in_buffer) {
    static int visit_times_since_last_boot_ = 0;
    std::unique_lock<std::mutex> lock(mutex_);
    // count up visitors.
    snprintf(resp_, sizeof(resp_), kRespFormat, ++visit_times_since_last_boot_);
    return 0;
}

const char *NetSceneGetIndexPage::ContentType() { return http::HeaderField::kTextPlain; }

void *NetSceneGetIndexPage::Data() { return resp_; }

size_t NetSceneGetIndexPage::Length() { return strlen(resp_); }

const char *NetSceneGetIndexPage::Route() { return kUrlRoute; }
```

注: 推荐使用 `ProtoBuf`。一些预定义好的protobuf `.proto` 文件存放于 `/protos/`。你可以运行：
```shell
cd framework/script
bash autogen.sh
```
来生成protobuf的C++文件，参阅 `NetSceneHelloSvr.cc` 作为示例以获取更多信息。


### WebSocket 长链接
Http协议虽可以保持长链接，但它不提供服务端向客户端主动推送消息的能力，使用 `WebSocket` 以填补此空缺。



## ✨ 示例工程
[Plant-Recognition-Server](https://github.com/xingyuuchen/object-identify-SVR.git) 是一个用于识别植物照片服务端程序，底层正是通过 `unixtar` 提供基本的网络通信能力。


## ✨ 反向代理
你可以通过以下命令来启动一个反向代理服务器：
```shell
cd framework/script
bash launchproxy.sh
```

反向代理完成以下工作:
* 转发。将Http请求包转发到可用的应用服务节点，并向客户转发回包。
* 负载均衡。你可以选择多个负载均衡策略：`Poll`, `By weight`, `IP Hash`。
* 注册中心。接受应用节点的心跳包，维护他们的状态。

通过修改 `reverseproxy/proxyserverconf.yml` ，配置你的反向代理，你可以定制：
* 反向代理监听的端口。
* 初始化所有的可用应用节点信息。
* 支持的最大连接数。
* 处理网络事件的线程数。

