#ifndef __LEPTONET_MALLOC_HOOK_H__
#define __LEPTONET_MALLOC_HOOK_H__

#include <stdint.h>
#include <stddef.h>

size_t leptonet_memory_usage_handle(uint32_t handle);
uint64_t leptonet_memory_usage();
uint64_t leptonet_memory_blocks();

// for debug
void* dleptonet_malloc(uint32_t handle, size_t sz);
void dleptonet_free(void* ptr);
size_t dleptonet_malloc_memory_usage(void* ptr, uint32_t *handle);


#endif
