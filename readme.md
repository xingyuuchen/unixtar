# Unixtar

(short for UniqueStar✨🌟✨)

The framework is a high-performance HTTP Server on unix based system.

Without using any third-party libraries, the framework writes from unix system calls and standard C library functions.

The framework adopts the model of `Epoll + NetThreads + WorkerThreads`.


## Build (unix)
```bash
git clone --recursive https://github.com/xingyuuchen/unixtar.git framework
cd framework/scripts
bash autogen.sh   # before this step, make sure Protobuf is installed.
bash cmake.sh -r -d   # -d: build will run as a daemon process, logs redirected to file using linux rsyslog, instead of stdout.
```

## Example Usage
Each network interface is represented by a class. They all inherit indirectly from `NetSceneBase`.

If you use ProtoBuf to serialize your data, inherit from `NetSceneProtoBuf` and use the POST request.

Else, to customize your network communication protocol, inherit from `NetSceneCustom`. Currently only GET requests are supported.

After defining your network interface classes and implement your business logic,
please register your class to the framework:
`NetSceneDispatcher::Instance()::RegisterNetScene<NetScene_YourBusiness>()`.

You can change server config by changing `serverconfig.yml`. After configuration, call `Server::Instance().serve()`, and the service just gets started!

Hope you enjoy :)

```c++
#include "log.h"
#include "webserver.h"
#include "netscenedispatcher.h"
#ifdef DAEMON
#include "daemon.h"
#endif

int main(int ac, char **argv) {
#ifdef DAEMON
    if (Daemon::Daemonize() < 0) {
        printf("Daemonize failed\n");
        return 0;
    }
#endif
    
    logger::OpenLog(argv[0]);

    LogI("Launching Server...")
    
    // NetScene_YourBusiness must inherit from NetSceneBase, which is your
    // predefined network interface (i.e. A specific Http url route)
    // See class NetSceneGetIndexPage below for detail.
    NetSceneDispatcher::Instance().RegisterNetScene<NetScene_YourBusiness>();
    NetSceneDispatcher::Instance().RegisterNetScene<NetScene_YourBusiness1>();
    NetSceneDispatcher::Instance().RegisterNetScene<NetScene_YourBusiness2>();
    
    WebServer::Instance().Serve();
    
    LogI("Server Down")
    return 0;
}
```

#### Here is an example of a network interface class:
```c++
#pragma once
#include "netscenecustom.h"
#include <mutex>

class NetSceneGetIndexPage : public NetSceneCustom {
  public:
    NetSceneGetIndexPage();

    // The unique NetScene type.
    int GetType() override;

    // New instance of your NetScene here.
    NetSceneBase *NewInstance() override;

    // Your business logic here.
    int DoSceneImpl(const std::string &_in_buffer) override;
    
    // Http body pointer.
    void *Data() override;

    // How long is your http body.
    size_t Length() override;

    // Http url route.
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
    // count the visitors.
    snprintf(resp_, sizeof(resp_), kRespFormat, ++visit_times_since_last_boot_);
    return 0;
}

const char *NetSceneGetIndexPage::ContentType() { return http::HeaderField::kTextPlain; }

void *NetSceneGetIndexPage::Data() { return resp_; }

size_t NetSceneGetIndexPage::Length() { return strlen(resp_); }

const char *NetSceneGetIndexPage::Route() { return kUrlRoute; }
```

## Example Project
[Plant-Recognition-Server](https://github.com/xingyuuchen/object-identify-SVR.git)
is a http server project, under the hood it is `unixtar` who provides basic network capacity.
