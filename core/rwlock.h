#ifndef __LEPTONET_RWLOCK_H__
#define __LEPTONET_RWLOCK_H__

#include "atomic.h"

struct rwlock {
  ATOMIC_INT read;
  ATOMIC_INT write;
};

inline void rwlock_init(struct rwlock *lock) {
  ATOMIC_INIT(&lock->read, 0);
  ATOMIC_INIT(&lock->write, 0);
}

inline void rwlock_rlock(struct rwlock *lock) {
  for(;;) {
    // wait for write lock to be released
    while(ATOMIC_CAP(&lock->write, 0, 1)) {
      while(ATOMIC_LOAD(&lock->write)) {}
    }
    ATOMIC_INC(&lock->read);
    // check if write lock is released
    if(ATOMIC_LOAD(&lock->write)) {
      ATOMIC_DEC(&lock->read);
    } else {
      break;
    }
  }
}

inline void rwlock_wlock(struct rwlock *lock) {
  // wait for write lock to be released
  while(ATOMIC_CAP(&lock->write, 0, 1)) {
    while(ATOMIC_LOAD(&lock->write)) {}
  }
  // wait for read lock to be released
  while(ATOMIC_LOAD(&lock->read)) {}
  ATOMIC_INC(&lock->write);
}

inline void rwlock_runlock(struct rwlock *lock) {
  ATOMIC_DEC(&lock->read);
}

inline void rwlock_wunlock(struct rwlock *lock) {
  ATOMIC_STORE(&lock->write, 0);
}

#endif
