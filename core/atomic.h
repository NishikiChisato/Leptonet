#ifndef __LEPTONET_ATOMIC_H__
#define __LEPTONET_ATOMIC_H__

#include <stdbool.h>
#include <stdint.h>

#define ATOMIC_INT volatile int
#define ATOMIC_PTR volatile uintptr_t
#define ATOMIC_LL volatile long long
#define ATOMIC_ULL volatile unsigned long long
#define ATOMIC_SZ volatile size_t
#define ATOMIC_BOOL volatile bool

#define ATOMIC_INIT(ptr, val) (*ptr = (val))
#define ATOMIC_LOAD(ptr) (*ptr)
#define ATOMIC_STORE(ptr, val) (*ptr = val)
#define ATOMIC_INC(ptr) __sync_add_and_fetch(ptr, 1)
#define ATOMIC_DEC(ptr) __sync_sub_and_fetch(ptr, 1)
#define ATOMIC_ADD(ptr, val) __sync_add_and_fetch(ptr, val)
#define ATOMIC_SUB(ptr, val) __sync_sub_and_fetch(ptr, val)
#define ATOMIC_AND(ptr, val) __sync_and_and_fetch(ptr, val)
#define ATOMIC_OR(ptr, val) __sync_or_and_fetch(ptr, val)
#define ATOMIC_XOR(ptr, val) __sync_xor_and_fetch(ptr, val)

#define ATOMIC_CAS(ptr, oval, nval) __sync_bool_compare_and_swap(ptr, oval, nval)

#endif
