#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "malloc_hook.h"
#include "leptonet_malloc.h"
#include "leptonet_server.h"
#include "atomic.h"

struct mem_hunk {
  ATOMIC_ULL handle;
  ATOMIC_SZ allocated;
};

#define MEM_ALLOCATED 0x20250101
#define MEM_RELEASED 0x20251010

struct mem_cookie {
  size_t mem_size;      // size of memory
  uint32_t handle;      // handle
  uint32_t dummy_tag;
  uint32_t cookie_size; // cookie size, should be placed in last
};

// (1 << 17)
#define SLOT_SIZE 0x10000
#define PREFIX_SIZE (sizeof(struct mem_cookie))

static struct mem_hunk slots[SLOT_SIZE];

static ATOMIC_ULL _mem_usage = 0;
static ATOMIC_ULL _mem_blocks = 0;

static inline uint32_t get_cookie_size(void *ptr) {
  uint32_t size;
  memcpy(&size, ptr - sizeof size, sizeof size);
  return size;
}

static inline ATOMIC_SZ* get_handle_allocated(uint32_t handle) {
  // due to we don't take lock, so we must retrive data carefully
  uint32_t hash = handle % SLOT_SIZE;
  struct mem_hunk* mem = &slots[hash];
  // mem may be accessed by different thread at the same time, so here we need implement a simple spinlock
  uint32_t oldhandle = mem->handle;
  size_t oldallocated = mem->allocated;
  if (oldhandle == 0 || oldallocated == 0) {
    // this mem don't have owner
    if (!ATOMIC_CAS(&mem->handle, oldhandle, handle)) {
      // update value failed
      return NULL;
    }
    if (!ATOMIC_CAS(&mem->allocated, oldallocated, 0)) {
      return NULL;
    }
  }
  if (mem->handle != handle) {
    // this mem has owner but inconsist with current handle
    return NULL;
  }
  return &mem->allocated;
}

static inline void track_memory_stat_alloc(uint32_t handle, size_t sz) {
  ATOMIC_ADD(&_mem_usage, sz);
  ATOMIC_INC(&_mem_blocks);
  ATOMIC_SZ *allocated = get_handle_allocated(handle);
  if (allocated) {
    ATOMIC_ADD(allocated, sz);
  }
}

static inline void track_memory_stat_free(uint32_t handle, size_t sz) {
  ATOMIC_SUB(&_mem_usage, sz);
  ATOMIC_DEC(&_mem_blocks);
  ATOMIC_SZ *allocated = get_handle_allocated(handle);
  if (allocated) {
    ATOMIC_SUB(allocated, sz);
  }
}

static inline void* fill_prefix(void * ptr, size_t size, uint32_t cookie_size) {
  uint32_t handle = leptonet_context_current_handle();
  struct mem_cookie* mem = ptr;
  mem->handle = handle;
  mem->mem_size = size;
  mem->dummy_tag = MEM_ALLOCATED;
  void *ret = ptr + cookie_size;
  memcpy(ret - sizeof(uint32_t), &cookie_size, sizeof (cookie_size));
  track_memory_stat_alloc(handle, size);
  return ret;
}

static inline void* clear_prefix(void *ptr, uint32_t cookie_size) {
  uint32_t prefix_size = get_cookie_size(ptr);
  struct mem_cookie *mem = (struct mem_cookie*)((char*)ptr - prefix_size);
  assert(mem->dummy_tag == MEM_ALLOCATED);
  mem->dummy_tag = MEM_RELEASED;
  track_memory_stat_free(mem->handle, mem->mem_size);
  return mem;
}

void* leptonet_malloc(size_t sz) {
  void *ptr = malloc(sz + PREFIX_SIZE);
  return fill_prefix(ptr, sz, PREFIX_SIZE);
}

void leptonet_free(void* ptr) {
  void* p = clear_prefix(ptr, PREFIX_SIZE);
  free(p);
}

static inline void* dfill_prefix(uint32_t handle, void * ptr, size_t size, uint32_t cookie_size) {
  struct mem_cookie* mem = ptr;
  mem->handle = handle;
  mem->mem_size = size;
  mem->dummy_tag = MEM_ALLOCATED;
  void *ret = ptr + cookie_size;
  memcpy(ret - sizeof(uint32_t), &cookie_size, sizeof (cookie_size));
  track_memory_stat_alloc(handle, size);
  return ret;
}

void* dleptonet_malloc(uint32_t handle, size_t sz) {
  void *ptr = malloc(sz + PREFIX_SIZE);
  return dfill_prefix(handle, ptr, sz, PREFIX_SIZE);
}

void dleptonet_free(void* ptr) {
  void* p = clear_prefix(ptr, PREFIX_SIZE);
  free(p);
}

size_t dleptonet_malloc_memory_usage(void* ptr, uint32_t *handle) {
  uint32_t prefix_size = get_cookie_size(ptr);
  struct mem_cookie *mem = (struct mem_cookie*)((char*)ptr - prefix_size);
  ATOMIC_SZ *size = get_handle_allocated(mem->handle);
  if (size) {
    *handle = mem->handle;
    return *size;
  }
  return 0;
}

size_t leptonet_memory_usage_handle(uint32_t handle) {
  ATOMIC_SZ *size = get_handle_allocated(handle);
  if (size) {
    return *size;
  }
  return 0;
}

uint64_t leptonet_memory_usage() {
  return ATOMIC_LOAD(&_mem_usage);
}

uint64_t leptonet_memory_blocks() {
  return ATOMIC_LOAD(&_mem_blocks);
}

