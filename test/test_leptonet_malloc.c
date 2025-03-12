#include <stdbool.h>
#include <time.h>
#include <assert.h>
#include <pthread.h>

#include "framework.h"
#include "../core/malloc_hook.h"
#include "../core/leptonet_malloc.h"

void check_handle(void *ptr, uint32_t handle, size_t sz) {
  uint32_t mem_handle = 0;
  size_t mem_sz = dleptonet_malloc_memory_usage(ptr, &mem_handle);
  if (mem_handle != (uint32_t)handle && mem_sz != (uint32_t)sz) {
    printf("mem_handle: %d, handle: %d\nmem_sz: %zu, sz: %zu\n", mem_handle, handle, mem_sz, sz);
    fflush(stdout);
    assert(false);
  }
}

bool test_basic() {
  TEST_BEGIN;

  void *p1 = dleptonet_malloc(1, 1024);
  void *p2 = dleptonet_malloc(2, 512);
  void *p3 = dleptonet_malloc(3, 1024);
  void *p4 = dleptonet_malloc(4, 512);

  ASSERT_NE(NULL, p1);
  check_handle(p1, 1, 1024);
  ASSERT_NE(NULL, p2);
  check_handle(p2, 2, 512);
  ASSERT_NE(NULL, p3);
  check_handle(p3, 3, 1024);
  ASSERT_NE(NULL, p4);
  check_handle(p4, 4, 512);

  ASSERT_EQ(4, leptonet_memory_blocks());
  ASSERT_EQ(1024 * 2 + 512 * 2, leptonet_memory_usage());

  dleptonet_free(p1);
  dleptonet_free(p2);
  dleptonet_free(p3);
  dleptonet_free(p4);

  TEST_END;
}

bool test_basic_loop() {
  TEST_BEGIN;

  const int loop = 10000;

  for (int i = 0; i < loop; i ++) {
    void *p1 = dleptonet_malloc(1, 1024);
    void *p2 = dleptonet_malloc(2, 512);
    void *p3 = dleptonet_malloc(3, 1024);
    void *p4 = dleptonet_malloc(4, 512);

    ASSERT_NE(NULL, p1);
    check_handle(p1, 1, 1024);
    ASSERT_NE(NULL, p2);
    check_handle(p2, 2, 512);
    ASSERT_NE(NULL, p3);
    check_handle(p3, 3, 1024);
    ASSERT_NE(NULL, p4);
    check_handle(p4, 4, 512);

    ASSERT_EQ(4, leptonet_memory_blocks());
    ASSERT_EQ(1024 * 2 + 512 * 2, leptonet_memory_usage());

    dleptonet_free(p1);
    dleptonet_free(p2);
    dleptonet_free(p3);
    dleptonet_free(p4);
  }

  TEST_END;
}

bool test_sequence_order() {
  TEST_BEGIN;

  const int limit = 10000;
  void *ptrs[limit];
  for (int i = 0; i < limit; i ++) {
    ptrs[i] = dleptonet_malloc(i, i * 4);
    ASSERT_NE(NULL, ptrs[i]);
    check_handle(ptrs[i], i, i * 4);
  }
  for (int i = 0; i < limit; i ++) {
    dleptonet_free(ptrs[i]);
  }

  TEST_END;
}

bool test_random_order() {
  TEST_BEGIN;

  srand(time(NULL));
  const int limit = 10000;
  void *ptrs[limit];
  memset(ptrs, 0, sizeof ptrs);
  for (int i = 0; i < limit; i ++) {
    int idx = rand() % limit;
    int count = 0;
    while (ptrs[idx % limit] == NULL) {
      if (count >= limit) {
        break;
      }
      idx = (idx + 1) % limit;
      count++;
    }
    if (count >= limit) {
      break;
    }
    if (ptrs[idx] == NULL) {
      ptrs[idx] = dleptonet_malloc(idx, idx * 4);
      ASSERT_NE(NULL, ptrs[idx]);
      check_handle(ptrs[idx], idx, idx * 4);
    }
  }
  // randomly free 50%
  for (int i = 0; i < limit / 2; i ++) {
    int idx = rand() % limit;
    if (ptrs[idx]) {
      dleptonet_free(ptrs[idx]);
      ptrs[idx] = NULL;
    }
  }
  // free all
  for (int i = 0; i < limit; i ++) {
    if (ptrs[i]) {
      dleptonet_free(ptrs[i]);
      ptrs[i] = NULL;
    }
  }

  TEST_END;
}

void* thread_func1(void* arg) {
  (void)arg;

  const int limit = 10000;
  for (int i = 0; i < limit; i ++) {
    void *p = dleptonet_malloc(i, i * 4);
    ASSERT_NE(NULL, p);
    check_handle(p, i, i * 4);
    dleptonet_free(p);
  }
  return NULL;
}

bool test_multithread() {
  TEST_BEGIN;

  const int limit = 5;
  pthread_t threads[limit];
  for (int i = 0; i < limit; i ++) {
    pthread_create(&threads[i], NULL, thread_func1, NULL);
  }
  for (int i = 0; i < limit; i ++) {
    pthread_join(threads[i], NULL);
  }

  TEST_END;
}

TEST_REGIST(test_leptonet_malloc, basic, test_basic);
TEST_REGIST(test_leptonet_malloc, basic_loop, test_basic_loop);
TEST_REGIST(test_leptonet_malloc, sequence_order, test_sequence_order);
TEST_REGIST(test_leptonet_malloc, random_order, test_random_order);
TEST_REGIST(test_leptonet_malloc, multithread, test_multithread);
