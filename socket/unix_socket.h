#ifndef OI_SVR_UNIX_SOCKET_H
#define OI_SVR_UNIX_SOCKET_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>


#define SOCKET int
#define INVALID_SOCKET (-1)
#define IS_EAGAIN(errno) ((errno) == EAGAIN || (errno) == EWOULDBLOCK)
#define CLOSE_SOCKET ::close

inline int SetNonblocking(SOCKET _fd) {
    int old_flags = ::fcntl(_fd, F_GETFL);
    if (::fcntl(_fd, F_SETFL, old_flags | O_NONBLOCK) == -1) {
        return -1;
    }
    return 0;
}

#endif //OI_SVR_UNIX_SOCKET_H
