# Unixtar

(short for Unique Star)

### This framework is a high-performance HTTP Server on unix based system.

### Without using any third-party libraries, the framework writes from unix system calls and standard C library functions.

### The framework adopts the model of Epoll + NetThreads + WorkerThreads.


## Unix
```bash
git clone --recursive https://github.com/xingyuuchen/unixtar.git
cd Unixtar/scripts
bash autogen.sh   # before this step, make sure Protobuf is installed.
bash cmake.sh -r -d   # -d: run as a daemon, logs will be redirect to files using linux rsyslog, instead of stdout.
```

## Windows
not support

