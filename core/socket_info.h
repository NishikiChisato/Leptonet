#ifndef __LEPTONET_SOCKET_BUFFER_H__
#define __LEPTONET_SOCKET_BUFFER_H__

#include <stdint.h>
#include <stdbool.h>

struct socket_info {
  // socket properties
  int id;           // unique socket id
  int opaque;       // user data
  uint64_t wb_size; // write buffer size
  // socket status
  bool read;        // read flag 
  bool write;       // write flag
  bool close;       // close flag
  bool error;       // error flag
  // statistics
  uint64_t rbytes;  // read bytes
  uint64_t wbytes;  // write bytes
  uint64_t rtime;   // read time
  uint64_t wtime;   // write time
  // human readable
  char name[256]; // socket name -> ip:port
};

#endif
