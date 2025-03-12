#include "framework.h"

bool test_sample() {
  TEST_BEGIN;

  ASSERT_EQ(1, 1);

  TEST_END;
}

TEST_REGIST(sample_suite, sample_name, test_sample);
