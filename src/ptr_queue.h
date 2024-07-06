#ifndef __ptr_queue_H__
#define __ptr_queue_H__

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  void **queue;
  int front;
  int rear;
  int size;
  int max_size;
  pthread_mutex_t mutex;
  pthread_cond_t full;
  pthread_cond_t empty;
} ptr_queue_t;

void ptr_queue_init(ptr_queue_t *queue, int max_size);

int ptr_queue_enqueue(ptr_queue_t *queue, void *data,
                        int timeout_ms);

void *ptr_queue_dequeue(ptr_queue_t *queue, int timeout_ms);

void ptr_queue_cleanup(ptr_queue_t *queue);

#ifdef __cplusplus
}
#endif

#endif /*__ptr_queue_H__*/
