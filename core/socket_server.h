#ifndef __LEPTONET_SOCKET_SERVER_H__
#define __LEPTONET_SOCKET_SERVER_H__

#include <stddef.h>
#include <stdbool.h>

#include "socket_info.h"
#include "spinlock.h"
#include "epoll.h"

// has data to receive
#define SOCKET_DATA 0
// open a new socket
#define SOCKET_OPEN 1
// accept a client connection
#define SOCKET_ACCEPT 2
// socket closed
#define SOCKET_CLOSE 3
// socket has error
#define SOCKET_ERR 4

struct socket_buffer {
  int id;     // unique socket id
  char *buffer;  // buffer
  int sz;     // buffer size
};

struct socket_message {
  int id;           // unique socket id
  uintptr_t opaque; // user data
  char *buffer;     // for SOCKET_OPEN, which is ip addr
  size_t ud;        // for SOCKET_DATA, which is buffer size
};

struct socket;
struct socket_server;

struct socket_server* socket_server_create(uint64_t time);

void socket_server_release(struct socket_server *ss);
int socket_server_poll(struct socket_server *ss, struct socket_message *sm);

void socket_server_listen(struct socket_server *ss, const char *host, const char *port, int backlog, uintptr_t opaque);
void socket_server_close(struct socket_server *ss, int id, int what, uintptr_t opaque);

void socket_server_sendhigh(struct socket_server *ss, struct socket_buffer *buf);
void socket_server_sendlow(struct socket_server *ss, struct socket_buffer *buf);

#endif
