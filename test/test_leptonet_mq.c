#include "framework.h"
#include "../core/leptonet_mq.h"

bool test_mq_basic() {
  TEST_BEGIN;

  leptonet_global_message_queue_init();
  leptonet_global_message_queue_release();
  TEST_END;
}

TEST_REGIST("mqtest", "basic", test_mq_basic);
