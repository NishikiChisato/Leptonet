#ifndef __LEPTONET_MQ_H__
#define __LEPTONET_MQ_H__

#include <stdint.h>
#include <stddef.h>

struct leptonet_message {
  uint32_t type;
  uint32_t sission; 
  char *data;
  size_t sz;
};

struct message_queue;

void leptonet_global_message_queue_init();
void leptonet_global_message_queue_release();
struct message_queue* leptonet_mq_create(uint32_t handle);

typedef void (*message_drop)(struct leptonet_message *, void *);

void leptonet_mq_release(struct message_queue *mq, message_drop drop, void * ud);

int leptonet_mq_length(struct message_queue *mq);

void leptonet_mq_push(struct message_queue *mq, struct leptonet_message *msg);
int leptonet_mq_pop(struct message_queue *mq, struct leptonet_message *msg);

void leptonet_globalmq_push(struct message_queue *mq);
int leptonet_globalmq_pop(struct message_queue *mq);


#endif
