#include <stdlib.h>

#include "framework.h"

#define MAX_TESTCASE_NUM 256

static struct TestInfo test_list[MAX_TESTCASE_NUM];
static int test_cnt = 0;

void testinfo_regist(const char *suite, const char *name, TestFunc func) {
  struct TestInfo *t = &test_list[test_cnt++];
  t->suite = suite;
  t->name = name;
  t->func = func;
  t->next = test_list;
}

int main() {
  int passed = 0;
  int failed = 0;

  for (int i = 0; i < test_cnt; i ++) {
    struct TestInfo *t = &test_list[i];
    printf(COLOR_YELLO "Running %s.%s\n" COLOR_RESET, t->suite, t->name);
    bool result = t->func();
    if (result) {
      printf(COLOR_GREEN "PASSED\n" COLOR_RESET);
      passed++;
    } else {
      printf(COLOR_RED "FAILED\n" COLOR_RESET);
      failed++;
    }
  }
  printf("==================\n");
  printf(COLOR_GREEN "Tests Passed: %d\n" COLOR_RESET, passed);
  printf(COLOR_RED "Tests Failed: %d\n" COLOR_RESET, failed);
  return failed > 0 ? 1 : 0;
}
