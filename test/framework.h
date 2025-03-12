#ifndef __LEPTONET_TEST_FRAMEWORK_H__
#define __LEPTONET_TEST_FRAMEWORK_H__

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

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
};

void testinfo_regist(const char *suite, const char *name, TestFunc func);

#define ASSERT_EQ(expected, acutal)                                                                                           \
  do {                                                                                                                        \
    assert(expected == acutal);                                                                                               \
  } while(0)

#define ASSERT_NE(expected, acutal)                                                                                           \
  do {                                                                                                                        \
    assert(expected != acutal);                                                                                               \
  } while(0)

#define TEST_BEGIN
#define TEST_END return true

// run this function before main starts
#define TEST_REGIST(suite, name, func) \
  static void __attribute__((constructor)) suite##name##register_test() { \
    testinfo_regist(#suite, #name, func);\
  }

#endif
