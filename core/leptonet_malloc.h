#ifndef __LEPTONET_MALLOC_H__
#define __LEPTONET_MALLOC_H__

#include <stddef.h>

#define leptonet_malloc malloc
#define leptonet_free free
#define leptonet_realloc realloc

void* leptonet_malloc(size_t);
void leptonet_free(void*);
void* leptonet_realloc(void*, size_t);

#endif
