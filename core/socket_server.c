#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

#include "epoll.h"
#include "socket_server.h"
#include "leptonet_malloc.h"

// socket server properties
#define SOCKET_IDMAX (1 << 16)
#define EVENT_MAX 256

#define TMP_PACKAGE_SIZE 256
#define TCP_MIN_READBYTES 64

// socket status
#define SOCKET_TYPE_INVALID 0
#define SOCKET_TYPE_RESERVE 1
#define SOCKET_TYPE_LISTEN 2
#define SOCKET_TYPE_ACCEPT 3
// for client socket
#define SOCKET_TYPE_CONNECTED 4
#define SOCKET_TYPE_HALFCLOSE_WRITE 5
#define SOCKET_TYPE_HALFCLOSE_READ 6

// for internal used
#define SOCKET_MORE 1

struct socket_stat {
  uint64_t rbytes;
  uint64_t wbytes;
  uint64_t lrtime;  // last read time
  uint64_t lwtime;  // last write time
};

// contain ipv4 and ipv6 address
union socketaddr {
  struct sockaddr addr;
  struct sockaddr_in addrv4;
  struct sockaddr_in6 addrv6;
};

struct write_buffer {
  void *buffer;
  char *ptr;
  size_t sz;
  struct write_buffer *next;
};

struct write_list {
  struct write_buffer *head;
  struct write_buffer *tail;
};

struct socket {
  int id;                     // unique socket id
  int fd;                     // socket file descriptor
  uintptr_t opaque;           // user data
  int socket_type;            // socket type(SOCK_STREAM, SOCK_DGRAM)
  int protocol;               // IPPROTO_TCP, IPPROTO_UDP
  int status;                 // socket status
  bool read;                  // read flag
  bool write;                 // write flag
  bool closing;               // user close flag

  struct socket_stat stat;    // socket statistics

  struct write_list high;     // high priority write list
  struct write_list low;      // low priority write list
  size_t wb_size;             // total size of write buffer

  int minread;                // for tcp, min read bytes, may grow up by the power of two
};

struct socket_server {
  int epfd;                           // epoll file descriptor
  int evid;                           // current handled event id
  int evnum;                          // all active events

  int checkctrl;                      // server side control, whether we should check pipe
  int sendctrl;                       // pipe send side control
  int recvctrl;                       // pipe recv side control
  fd_set rfds;                        // for select

  int reserved;                       // reserved socket id, for EMFILE

  int allocated;               // allocated unique socket id

  uint64_t time;                      // timestemp
  struct spinlock lock;               // lock

  struct socket slots[SOCKET_IDMAX];  // socket slots
  struct event events[EVENT_MAX];     // epoll events
  char *tmpbuf[TMP_PACKAGE_SIZE];     // temporary buffer
};

// if success, return zero
static inline int enable_read(struct socket_server *ss, struct socket *s, bool read) {
  if (s->read == read) {
    return 0;
  }
  s->read = read;
  return epread(ss->epfd, s->fd, s, read);
}

// if success, return zero
static inline int enable_write(struct socket_server *ss, struct socket *s, bool write) {
  if (s->write == write) {
    return 0;
  }
  s->write = write;
  return epwrite(ss->epfd, s->fd, s, write);
}

static inline void enable_nonblocking(struct socket *s) {
  int flag = fcntl(s->fd, F_GETFL);
  fcntl(s->fd, F_SETFL, flag | O_NONBLOCK);
}

static inline void stat_init(struct socket_stat *st) {
  memset(st, 0, sizeof *st);
}

static inline void stat_read(struct socket_stat *st, uint64_t time, uint64_t bytes) {
  assert(time >= st->lrtime);
  st->lrtime = time;
  st->rbytes += bytes;
}

static inline void stat_write(struct socket_stat *st, uint64_t time, uint64_t bytes) {
  assert(time >= st->lwtime);
  st->lwtime = time;
  st->wbytes += bytes;
}

static inline void write_buffer_free(struct write_buffer *wb) {
  assert(wb && wb->buffer);
  leptonet_free(wb->buffer);
  leptonet_free(wb);
}

static inline void write_list_push_head(struct write_list *wl, struct write_buffer *wb) {
  if (wl->head == NULL) {
    assert(wl->tail == NULL);
    wl->head = wl->tail = wb; 
  } else {
    wl->head = wb->next;
    wl->head = wb;
  }
}

static inline void write_list_push_tail(struct write_list *wl, struct write_buffer *wb) {
  if (wl->head == NULL) {
    assert(wl->tail == NULL);
    wl->head = wl->tail = wb; 
  } else {
    assert(wl->tail);
    wl->tail->next = wb;
    wl->tail = wb;
  }
}

static inline struct write_buffer* write_list_pop_head(struct write_list *wl) {
  if (wl->head == NULL) {
    assert(wl->tail == NULL);
    return NULL;
  } 
  struct write_buffer *tmp = wl->head;
  wl->head = tmp->next;
  if(wl->head == NULL) {
    wl->tail = NULL;
  }
  return tmp;
}

static inline void write_list_free(struct write_list *wl) {
  while (wl->head) {
    struct write_buffer *tmp = wl->head;
    wl->head = tmp->next;
    write_buffer_free(tmp);
  }
  wl->tail = NULL;
}

static inline void write_list_clear(struct write_list *wl) {
  wl->head = wl->tail = NULL;
}

static inline int write_list_empty(struct write_list *wl) {
  return wl->head == NULL && wl->tail == NULL;
}

static int reserved_id(struct socket_server *ss) {
  if (!spinlock_trylock(&ss->lock)) {
    return -1;
  }
  for (int i = 0; i < SOCKET_IDMAX; i ++){
    int newid = ++ss->allocated;
    if (newid < 0) {
      newid = 0;
      ss->allocated = 0;
    }
    struct socket *s = &ss->slots[newid];
    if (s->status == SOCKET_TYPE_INVALID) {
      s->id = newid;
      s->status = SOCKET_TYPE_RESERVE;
      spinlock_unlock(&ss->lock);
      return newid;
    }
  }
  spinlock_unlock(&ss->lock);
  return -1;
}

static struct socket* newsocket(struct socket_server *ss, int id, int fd, uintptr_t opaque, int type, int protocol) {
  if (!spinlock_trylock(&ss->lock)) {
    return NULL;
  }
  struct socket *s = &ss->slots[id];
  if (s->status != SOCKET_TYPE_RESERVE) {
    return NULL;
  }
  s->fd = fd;
  s->socket_type = type;
  s->opaque = opaque;
  s->protocol = protocol;
  s->read = false;
  s->write = false;
  s->closing = false;
  stat_init(&s->stat);
  s->minread = TCP_MIN_READBYTES;
  write_list_clear(&s->high);
  write_list_clear(&s->low);
  s->wb_size = 0;
  enable_nonblocking(s);
  epregist(ss->epfd, s->fd, s);
  if (enable_read(ss, s, true)) {
    sprintf(stderr, "[socket-server]: enable read for %d fd error: %s\n", s->fd, strerror(errno));
    return NULL;
  }
  return s;
}

struct request_close {
  uintptr_t opaque;
  int id;
  int what;
};

struct request_listen {
  uintptr_t opaque;
  const char *host;
  const char *port;
  int backlog;
};

struct request_send {
  int id;
  char *buf;
  size_t sz;
  bool high;
};

struct request_package {
  // first six bytes are dummy
  // idx: 6 -> type
  // idx: 7 -> len
  char header[8];
  union {
    char buf[256];
    struct request_close rclose;      // 'X'
    struct request_listen rlisten;    // 'L'
    struct request_send rsend;        // 'W'
  } u;
  // not used
  char dummy[256];
};

static void send_request(struct socket_server *ss, struct request_package *request, uint8_t type, uint8_t len) {
  request->header[6] = type;
  request->header[7] = len;
  char *buf = (char*)request + offsetof(struct request_package, header[6]);
  int cnt = write(ss->sendctrl, buf, len + 2);
  if (cnt < 0) {
    fprintf(stderr, "[socket_server]: send request error: %s\n", strerror(errno));
    return;
  }
  // atomic write
  assert(cnt == len + 2);
}

void socket_server_close(struct socket_server *ss, int id, int what, uintptr_t opaque) {
  struct request_package pkg;
  pkg.u.rclose.id = id;
  pkg.u.rclose.opaque = opaque;
  pkg.u.rclose.what = what;
  send_request(ss, &pkg, 'X', sizeof pkg.u.rclose);
}

static int try_listen(const char *host, const char *port, int backlog) {
  int status = 0;
  int fd = 0;
  struct addrinfo hints, *servinfo, *p;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  if ((status = getaddrinfo(host, port, &hints, &servinfo)) < 0) {
    sprintf(stderr, "[socket-server]: get address info failed: %s\n", strerror(errno));
    return -1;
  }

  for (p = servinfo; p; p = p->ai_next) {
    if ((fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) {
      sprintf(stderr, "[socket-server]: create socket failed: %s\n", strerror(errno));
      continue;
    }
    int tmp = 1;
    if ((status = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &tmp, sizeof tmp)) < 0) {
      close(fd);
      sprintf(stderr, "[socket-server]: set socket options failed: %s\n", strerror(errno));
      continue;
    }
    tmp = 1;
    if ((status = setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &tmp, sizeof tmp)) < 0) {
      close(fd);
      sprintf(stderr, "[socket-server]: set socket options failed: %s\n", strerror(errno));
      continue;
    }
    if ((status = bind(fd, p->ai_addr, p->ai_addrlen)) < 0) {
      close(fd);
      sprintf(stderr, "[socket-server]: bind socket failed: %s\n", strerror(errno));
      continue;
    }
    if ((status = listen(fd, backlog)) < 0) {
      close(fd);
      sprintf(stderr, "[socket-server]: listen socket failed: %s\n", strerror(errno));
      continue;
    }
    break;
  }
  freeaddrinfo(servinfo);
  if (p == NULL) {
    fprintf(stderr, "[socket-server]: failed to listen specific port: %s", strerror(errno));
    return -1;
  }
  return fd;
}

void socket_server_listen(struct socket_server *ss, const char *host, const char *port, int backlog, uintptr_t opaque) {
  /* int fd = try_listen(host, port, backlog); */
  /* int id = reserved_id(ss); */
  struct request_package pkg;
  pkg.u.rlisten.opaque = opaque;
  pkg.u.rlisten.host = host;
  pkg.u.rlisten.port = port;
  pkg.u.rlisten.backlog = backlog;
  send_request(ss, &pkg, 'L', sizeof pkg.u.rlisten);
}

void socket_server_sendhigh(struct socket_server *ss, struct socket_buffer *buf) {
  struct request_package pkg;
  pkg.u.rsend.id = buf->id;
  pkg.u.rsend.sz = buf->sz;
  pkg.u.rsend.buf = buf->buffer;
  pkg.u.rsend.high = true;
  send_request(ss, &pkg, 'W', sizeof pkg.u.rsend);
}

void socket_server_sendlow(struct socket_server *ss, struct socket_buffer *buf) {
  struct request_package pkg;
  pkg.u.rsend.id = buf->id;
  pkg.u.rsend.sz = buf->sz;
  pkg.u.rsend.buf = buf->buffer;
  pkg.u.rsend.high = false;
  send_request(ss, &pkg, 'W', sizeof pkg.u.rsend);
}

struct socket_server* socket_server_create(uint64_t time) {
  struct socket_server *ss = leptonet_malloc(sizeof *ss);
  memset(ss, 0, sizeof *ss);
  ss->epfd = epinit();
  if (!epvalid(ss->epfd)) {
    sprintf(stderr, "[socket-server]: epoll create failed: %s\n", strerror(errno));
    return NULL;
  }
  int pipefd[2];
  if (pipe(pipefd) < 0) {
    close(ss->epfd);
    sprintf(stderr, "[socket-server]: pipe create failed: %s\n", strerror(errno));
    return NULL;
  }
  ss->recvctrl = pipefd[0];
  ss->sendctrl = pipefd[1];
  ss->reserved = dup(1);

  if (ss->reserved < 0) {
    close(ss->epfd);
    close(ss->recvctrl);
    close(ss->sendctrl);
    sprintf(stderr, "[socket-server]: duplicate fd failed: %s\n", strerror(errno));
    return NULL;
  }
  FD_ZERO(&ss->rfds);
  spinlock_init(&ss->lock);
  ss->time = time;
  ss->checkctrl = 1;

  return ss;
}

static int force_close(struct socket_server *ss, struct socket *s) {
  // temporary set it to true
  s->closing = true;
  s->status = SOCKET_TYPE_INVALID;

  int hasdata = 0;

  if (!write_list_empty(&s->high)) {
    write_list_free(&s->high);
    hasdata = 1;
  }
  if (!write_list_empty(&s->low)) {
    write_list_free(&s->low);
    hasdata = 1;
  }

  write_list_clear(&s->high);
  write_list_clear(&s->low);
  s->wb_size = 0;
  s->minread = 0;

  enable_read(ss, s, false);
  enable_write(ss, s, false);
  epdel(ss->epfd, s->fd);
  close(s->fd);

  s->closing = false;
  if (hasdata == 1) {
    return SOCKET_ERR;
  }
  return SOCKET_CLOSE;
}

void socket_server_release(struct socket_server *ss) {
  spinlock_lock(&ss->lock);
  for (int i = 0; i < SOCKET_IDMAX; i ++) {
    struct socket *s = &ss->slots[i];
    if (s->status != SOCKET_TYPE_INVALID && s->status != SOCKET_TYPE_RESERVE) {
      // ignore return value
      force_close(ss, s);
    }
  }
  close(ss->epfd);
  close(ss->sendctrl);
  close(ss->recvctrl);
  if (ss->reserved > 0) {
    close(ss->reserved);
  }
  spinlock_unlock(&ss->lock);
  spinlock_destroy(&ss->lock);
  leptonet_free(ss);
}

static int hascmd(struct socket_server *ss) {
  struct timeval tv = {.tv_sec = 0, .tv_usec = 0};
  FD_SET(ss->recvctrl, &ss->rfds);
  int cnt = select(ss->recvctrl + 1, &ss->rfds, NULL, NULL, &tv);
  return cnt;
}

static int readfrompipe(int fd, char *buf, size_t sz) {
  int cnt = read(fd, buf, sz);
  if (cnt < 0) {
    if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
      return 0;
    }
    fprintf(stderr, "[socket-server] can't read from pipe: %s\n", strerror(errno));
    return -1;
  }
  assert(cnt == sz);
  return cnt;
}

static int report_close(struct socket_server *ss, struct request_close *rclose, struct socket_message *sm) {
  int id = rclose->id;
  struct socket *s = &ss->slots[id];
  int what = rclose->what;
  if (what == SHUT_RD) {
    s->status = SOCKET_TYPE_HALFCLOSE_READ;
    shutdown(s->fd, SHUT_RD);
  } else if (what == SHUT_WR) {
    s->status = SOCKET_TYPE_HALFCLOSE_WRITE;
    shutdown(s->fd, SHUT_WR);
  } else if (what == SHUT_RDWR) {
    force_close(ss, s);
  } else {
    fprintf(stderr, "[socket-server]: rclose: close type error\n");
    return SOCKET_ERR;
  }
  return SOCKET_CLOSE;
}

static int report_listen(struct socket_server *ss, struct request_listen *rlisten, struct socket_message *sm) {
  const char *host = rlisten->host;
  const char *port = rlisten->port;
  int backlog = rlisten->backlog;
  uintptr_t opaque = rlisten->opaque;

  int fd = try_listen(host, port, backlog);
  int id = reserved_id(ss);
  struct socket *s = newsocket(ss, id, fd, opaque, SOCK_STREAM, IPPROTO_TCP);

  s->status = SOCKET_TYPE_LISTEN;

  sm->opaque = opaque;
  sm->id = id;
  sm->buffer = NULL;
  sm->ud = 0;
  return SOCKET_OPEN;
}

static int report_send(struct socket_server *ss, struct request_send *rsend, struct socket_message *sm) {
  int id = rsend->id;
  bool high = rsend->high;

  struct socket *s = &ss->slots[id];
  struct write_buffer wb;
  wb.buffer = rsend->buf;
  wb.ptr = rsend->buf;
  wb.sz = rsend->sz;
  wb.next = NULL;
  if (high) {
    write_list_push_tail(&s->high, &wb);
  } else {
    write_list_push_tail(&s->low, &wb);
  }
  return -1;
}

static int process_cmd(struct socket_server *ss, struct socket_message *sm) {
  char header[2];
  char request_buf[256];
  int r;
  r = readfrompipe(ss->recvctrl, header, sizeof header);
  if (r == 0) {
    return -1;
  }
  r = readfrompipe(ss->recvctrl, request_buf, sizeof request_buf);
  if (r == 0) {
    return -1;
  }
  uint8_t type = header[0];
  uint8_t len = header[1];
  switch(type) {
    case 'X': {
      spinlock_lock(&ss->lock);
      r = report_close(ss, (struct request_close*)request_buf, sm);
      spinlock_unlock(&ss->lock);
      return r;
    }
    case 'L': {
      spinlock_lock(&ss->lock);
      r = report_listen(ss, (struct request_listen*)request_buf, sm);
      spinlock_unlock(&ss->lock);
      return r;
    }
    case 'W': {
      spinlock_lock(&ss->lock);
      r = report_send(ss, (struct request_send*)request_buf, sm);
      spinlock_unlock(&ss->lock);
      return r;
    }
  }
  return -1;
}

static int report_error(struct socket *s, struct socket_message *sm) {
  sm->id = s->id;
  sm->opaque = s->opaque;
  sm->ud = 0;
  sm->buffer = NULL;
  return SOCKET_ERR;
}

static int process_read_event(struct socket_server *ss, struct socket *s, struct socket_message *sm) {
  size_t sz = s->minread;
  char *buf = leptonet_malloc(sz);
  int cnt = recv(s->fd, buf, sz, 0);

  if (cnt < 0) {
    if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
      return -1;
    }
    return report_error(s, sm);
  }
  
  stat_read(&s->stat, ss->time, cnt);
  sm->id = s->id;
  sm->opaque = s->opaque;
  sm->ud = cnt;
  sm->buffer = buf;
  if (cnt == sz) {
    s->minread *= 2;
  } else if (cnt > TCP_MIN_READBYTES && 2 * cnt < sz) {
    s->minread /= 2;
  }
  return SOCKET_DATA;
}

static int send_writelist(struct socket *s, struct write_list *wl, struct socket_message *sm) {
  while (wl->head) {
    struct write_buffer *wb = wl->head;
    int cnt = send(s->fd, wb->ptr, wb->sz, 0);
    if (cnt < 0) {
      if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
        return -1;
      }
      return report_error(s, sm);
    }
    if (cnt != wb->sz) {
      wb->ptr += cnt;
      wb->sz -= cnt;
      return -1;
    }
    wl->head = wb->next;
    write_buffer_free(wb);
  }
  wl->tail = NULL;
  return -1;
}

static inline int write_list_uncomplete(struct write_list *wl) {
  if (write_list_empty(wl)) {
    return 0;
  }
  return wl->head->buffer != wl->head->ptr;
}

static void raise_writelist(struct socket *s) {
  struct write_buffer *wb = write_list_pop_head(&s->low);
  write_list_push_head(&s->high, wb);
}

// each socket has two write list: high and low
// if high list is not empty, send it as far as possible
// if high list is empty, send low list as far as possible
// after sending low list, if it's uncomplete, we raise it to high
static int process_write_event(struct socket_server *ss, struct socket *s, struct socket_message *sm) {
  if (!write_list_empty(&s->high)) {
    return send_writelist(s, &s->high, sm);
  }
  if (!write_list_empty(&s->high) && !write_list_empty(&s->low)) {
    return send_writelist(s, &s->low, sm);
  }
  if (write_list_uncomplete(&s->low)) {
    raise_writelist(s);
  }
  sm->id = s->id;
  sm->opaque = s->opaque;
  sm->ud = 0;
  sm->buffer = NULL;
  return -1;
}

int socket_server_poll(struct socket_server *ss, struct socket_message *sm) {
  for (;;) {
    if (ss->checkctrl) {
      if (hascmd(ss)) {
        int r = process_cmd(ss, sm);
        if (r == -1) {
          continue;
        }
        return r;
      } else {
        ss->checkctrl = 0;
      }
    }
    if (ss->evid == ss->evnum) {
      int cnt = epwait(ss->epfd, ss->events, EVENT_MAX);
      if (cnt < 0) {
        fprintf(stderr, "[socket-server]: epoll wait failed: %s\n", strerror(errno));
        continue;
      }
      ss->evnum = cnt;
      ss->checkctrl = 1;
    }
    struct event *e = &ss->events[ss->evid];
    struct socket *s = e->socket;
    switch(s->status) {
      case SOCKET_TYPE_INVALID:
      case SOCKET_TYPE_RESERVE: {
        int r = force_close(ss, s);
        if (r == SOCKET_ERR) {
          return report_error(s, sm);
        }
        break;
      }
      case SOCKET_TYPE_LISTEN: {

      }
    }
    if (e->read) {
      spinlock_lock(&ss->lock);
      int r = process_read_event(ss, s, sm);
      spinlock_unlock(&ss->lock);
      if (r == SOCKET_ERR) {
        fprintf(stderr, "[socket_server]: socket: %d error: %s\n", s->id, strerror(errno));
        return r;
      } else {
        ss->evid--;
        return r;
      }
    }
    if (e->write) {
      spinlock_lock(&ss->lock);
      int r = process_write_event(ss, s, sm);
      spinlock_unlock(&ss->lock);
      if (r == SOCKET_ERR) {
        fprintf(stderr, "[socket_server]: socket: %d error: %s\n", s->id, strerror(errno));
        return r;
      }
    }
    if (e->eof) {
      // remote has close write side, we should close read side
      if (s->status == SOCKET_TYPE_HALFCLOSE_READ) {
        break;
      }
      s->status = SOCKET_TYPE_HALFCLOSE_READ;
      if (enable_read(ss, s, false)) {
        fprintf(stderr, "[socket-server]: close read for %d failed: %s\n", s->id, strerror(errno));
        return report_error(s, sm);
      }
    }
    if (e->error) {
      // we retrive errors and log it
      int err = 0;
      socklen_t len = sizeof err;
      int r = getsockopt(s->fd, SOL_SOCKET, SO_ERROR, &err, &len);
      err = r < 0 ? errno : err;
      fprintf(stderr, "[socket-server]: socket error: %s\n", strerror(err));
      report_error(s, sm);
      return force_close(ss, s);
    }
  }
}
