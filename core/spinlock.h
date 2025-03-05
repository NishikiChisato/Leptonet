#ifndef __LEPTONET_SPINLOCK_H__
#define __LEPTONET_SPINLOCK_H__

#include "atomic.h"

struct spinlock {
  ATOMIC_INT lock;
};

inline void spinlock_init(struct spinlock *lock) {
  ATOMIC_INIT(&lock->lock, 0);
}

inline void spinlock_lock(struct spinlock *lock) {
  // return the old value and set the new value
  while(ATOMIC_CAP(&lock->lock, 0, 1)) {
    // reduce contention
    while(ATOMIC_LOAD(&lock->lock)) {}
  }
}

inline void spinlock_unlock(struct spinlock *lock) {
  // reset to zero
  __sync_lock_release(&lock->lock);
}

// return 1, lock success; otherwise, lock failed
inline int spinlock_trylock(struct spinlock *lock) {
  return __sync_lock_test_and_set(&lock->lock, 1) == 0;
}

inline void spinlock_destroy(struct spinlock *lock) {
  // do nothing
  (void)lock;
}

#endif
