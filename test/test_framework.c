#include <stdlib.h>

#include "test_framework.h"

static struct TestInfo *test_list = NULL;

void testinfo_regist(const char *suite, const char *name, TestFunc func) {
  struct TestInfo *t = malloc(sizeof *t);
  t->suite = suite;
  t->name = name;
  t->func = func;
  t->next = test_list;
  test_list = t;
}

int main() {
  int passed = 0;
  int failed = 0;

  for (struct TestInfo *t = test_list; t; t = t->next) {
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
