#ifndef __LEPTONET_EPOLL_H__
#define __LEPTONET_EPOLL_H__

#include <unistd.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>

struct event {
  void *socket;   // socket pointer
  bool read;      // read event
  bool write;     // write event
  bool error;     // error event
  bool eof;       // eof event
};

inline int epinit() {
  return epoll_create(1024);
}

inline void epclose(int epfd) {
  close(epfd);
}

inline int epvalid(int epfd) {
  return epfd >= 0;
}

inline int epregist(int epfd, int fd, void *ptr) {
  struct epoll_event e;  
  e.events = EPOLLIN;
  e.data.ptr = ptr;
  return epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &e);
}

inline int epread(int epfd, int fd, void *ptr, bool read) {
  struct epoll_event e;
  e.events = read ? EPOLLIN : 0;
  e.data.ptr = ptr;
  return epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &e);
}

inline int epwrite(int epfd, int fd, void *ptr, bool write) {
  struct epoll_event e;
  e.events = write ? EPOLLOUT : 0;
  e.data.ptr = ptr;
  return epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &e);
}

inline int epwait(int epfd, struct event *evs, int maxevents) {
  epoll_event events[maxevents];
  int cnt = epoll_wait(epfd, events, maxevents, 0);
  for (int i = 0; i < cnt; i ++) {
    evs[i].read = (events[i].events & EPOLLIN);
    evs[i].write = (events[i].events & EPOLLOUT);
    evs[i].error = (events[i].events & EPOLLERR);
    evs[i].eof = (events[i].events & EPOLLHUP);
    evs[i].socket = events[i].data.ptr;
  }
  return cnt;
}

inline int epdel(int epfd, int fd) {
  return epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL) == 0;
}

#endif
