#ifndef __LEPTONET_TEST_FRAMEWORK_H__
#define __LEPTONET_TEST_FRAMEWORK_H__

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// color
#define COLOR_RED "\x1b[31m"
#define COLOR_GREEN "\x1b[32m"
#define COLOR_YELLO "\x1b[33m"
#define COLOR_RESET "\x1b[0m"

typedef bool (*TestFunc)();

struct TestInfo {
  const char *suite;
  const char *name;
  TestFunc func;
  struct TestInfo *next;
};

void testinfo_regist(const char *suite, const char *name, TestFunc func);

#define ASSERT_EQ(expected, acutal)                                                                                           \
  do {                                                                                                                        \
    typeof(expected) e = (expected);                                                                                          \
    typeof(acutal) a = (acutal);                                                                                              \
    if(memcmp(&e, &a, sizeof(e)) != 0) {                                                                                      \
      printf(COLOR_RED "  ASSERT_EQ failed: Expected %d, got %d (%s:%d)\n" COLOR_RESET, (int)e, (int)a, __FILE__, __LINE__);  \
      return false;                                                                                                           \
    }                                                                                                                         \
  } while(0)

#define TEST_BEGIN
#define TEST_END return true

// run this function before main starts
#define TEST_REGIST(suite, name, func) \
  static void __attribute__((constructor)) register_test() { \
    testinfo_regist(suite, name, func);\
  }

#endif
