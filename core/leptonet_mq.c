#include <assert.h>

#include "leptonet_mq.h"
#include "leptonet_malloc.h"
#include "spinlock.h"

#define UNINGLOBAL 0
#define INGLOBAL 1

#define DEFAULT_MQ_SIZE 1024

struct message_queue {
  int head;
  int tail;
  int capacity;
  int in_global;      // indicate whether this mq is in global mq
  int32_t handle;     // indicate which context this mq belongs to
  struct message_queue *next;
  struct leptonet_message *msg;
  // TODO: we may add a fileds to indicate whether this queue has too many messages
};

struct global_message_queue {
  struct message_queue *head;
  struct message_queue *tail;
  struct spinlock lock;
};

static struct global_message_queue *Q;

void leptonet_global_message_queue_init() {
  struct global_message_queue *q = leptonet_malloc(sizeof *q); 
  spinlock_init(&q->lock);
  q->head = q->tail = NULL;
  Q = q;
}

void leptonet_global_message_queue_release() {
  struct message_queue *mq;
  struct leptonet_message msg;
  while(leptonet_globalmq_pop(mq)) {
    leptonet_mq_release(mq, NULL, NULL);
  }
  leptonet_free(Q);
}

struct message_queue* leptonet_mq_create(uint32_t handle) {
  struct message_queue *q = leptonet_malloc(sizeof *q);
  q->handle = handle;
  q->head = q->tail = 0;
  q->capacity = DEFAULT_MQ_SIZE;
  q->in_global = UNINGLOBAL;
  q->next = NULL;
  q->msg = leptonet_malloc(sizeof(struct leptonet_message) * q->capacity);
  return q;
}

void leptonet_mq_release(struct message_queue *mq, message_drop drop, void * ud) {
  assert(mq->in_global == UNINGLOBAL);
  struct leptonet_message msg;
  while (leptonet_mq_pop(mq, &msg)) {
    if (drop != NULL) {
      drop(&msg, ud);
    }
  }
}

int leptonet_mq_length(struct message_queue *mq) {
  int len = (mq->tail - mq->head + mq->capacity) % mq->capacity;
  return len;
}

static void extend_mq(struct message_queue *mq) {
  struct leptonet_message *newmsg = leptonet_realloc(mq->msg, mq->capacity * 2);
  mq->head = 0;
  mq->tail = mq->capacity;
  mq->capacity *= 2;
  leptonet_free(mq->msg);
  mq->msg = newmsg;
}

void leptonet_mq_push(struct message_queue *mq, struct leptonet_message *msg) {
  mq->msg[mq->tail++] = *msg;;

  if (mq->tail == mq->capacity) {
    mq->tail = 0;
  }
  
  if (mq->tail == mq->head) {
    extend_mq(mq);
  }
  // if this mq has message, we should push it into global mq
  spinlock_lock(&Q->lock);
  if (mq->in_global == UNINGLOBAL) {
    leptonet_globalmq_push(mq);
    mq->in_global = INGLOBAL;
  }
  spinlock_unlock(&Q->lock);
}

int leptonet_mq_pop(struct message_queue *mq, struct leptonet_message *msg) {
  assert(mq->head <= mq->tail);
  if (mq->head == mq->tail) {
    // a empty queue cannot be in global mq
    mq->in_global = UNINGLOBAL;
    return 0;
  }
  *msg = mq->msg[mq->head++];
  if (mq->head == mq->capacity) {
    mq->head = 0;
  }
  return 1;
}

void leptonet_globalmq_push(struct message_queue *mq) {
  spinlock_lock(&Q->lock);
  if (Q->head == NULL) {
    assert(Q->tail == NULL);
    Q->head = Q->tail = mq;
    mq->next = NULL;
  } else {
    assert(Q->tail);
    mq->next = Q->tail->next;
    Q->tail = mq;
  }
  spinlock_unlock(&Q->lock);
}

int leptonet_globalmq_pop(struct message_queue *mq) {
  spinlock_lock(&Q->lock);
  if (Q->head == NULL) {
    assert(Q->tail == NULL);
    spinlock_unlock(&Q->lock);
    return 0;
  } else {
    assert(Q->tail == NULL);
    mq = Q->head;
    Q->head = Q->head->next;
    if (Q->head == NULL) {
      Q->tail = NULL;
    }
  }
  spinlock_unlock(&Q->lock);
  return 1;
}
