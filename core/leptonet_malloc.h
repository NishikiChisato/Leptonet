#ifndef __LEPTONET_MALLOC_H__
#define __LEPTONET_MALLOC_H__

#include <stddef.h>
#include <stdlib.h>

#define leptonet_malloc cusmalloc
#define leptonet_free cusfree

void* leptonet_malloc(size_t);
void leptonet_free(void*);

void* leptonet_strdup(const char* name);

#endif
