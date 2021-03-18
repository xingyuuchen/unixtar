# SVR for object-identify
#### 工程不使用第三方库，网络请求从 System Call 和 Standard C Library Function 写起.
#### 使用 Epoll + ThreadPoll 实现高性能 IO.

## 编译方法
### UNIX
```bash
git clone --recursive https://github.com/xingyuuchen/object-identify-SVR.git
cd object-identify-SVR/scripts
bash autogen.sh
```
仅编译
```bash
bash cmake.sh
```
编译运行
```bash
bash cmake.sh -r
```
以守护进程形式运行
```bash
bash cmake.sh -r -d   # -d: run as a daemon.
```

### Windows
不支持
