#ifndef OI_SVR_BLOCKSOCKET_H
#define OI_SVR_BLOCKSOCKET_H

#include "socketpoll.h"
#include "unixsocket.h"
#include "autobuffer.h"


ssize_t BlockSocketSend(SOCKET _socket,
                       AutoBuffer &_send_buff,
                       size_t _buff_size,
                       int _timeout_mills = 5000,
                       bool _wait_full = false);


ssize_t BlockSocketReceive(SOCKET _socket,
                            AutoBuffer &_recv_buff,
                            SocketPoll &_socket_poll,
                            size_t _buff_size,
                            int _timeout_mills = 5000,
                            bool _wait_full = false);

#endif //OI_SVR_BLOCKSOCKET_H
