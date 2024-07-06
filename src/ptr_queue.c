#include "ptr_queue.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

void ptr_queue_init(ptr_queue_t *queue, int max_size) {
  queue->front = 0;
  queue->rear = 0;
  queue->size = 0;
  queue->max_size = max_size;
  queue->queue = (void **)malloc(max_size * sizeof(void *));
  pthread_mutex_init(&queue->mutex, NULL);
  pthread_cond_init(&queue->full, NULL);
  pthread_cond_init(&queue->empty, NULL);
}

int ptr_queue_enqueue(ptr_queue_t *queue, void *data, int timeout_ms) {
  struct timeval now;
  gettimeofday(&now, NULL);
  struct timespec ts;
  ts.tv_sec = now.tv_sec + timeout_ms / 1000;
  ts.tv_nsec = (now.tv_usec + (timeout_ms % 1000) * 1000) * 1000;
  ts.tv_sec += ts.tv_nsec / 1000000000;
  ts.tv_nsec %= 1000000000;

  pthread_mutex_lock(&queue->mutex);
  while (queue->size >= queue->max_size) {
    if (pthread_cond_timedwait(&queue->full, &queue->mutex, &ts) == ETIMEDOUT) {
      pthread_mutex_unlock(&queue->mutex);
      return 1; // Timeout reached
    }
  }
  queue->queue[queue->rear] = data;
  queue->rear = (queue->rear + 1) % queue->max_size;
  queue->size++;
  pthread_cond_signal(&queue->empty);
  pthread_mutex_unlock(&queue->mutex);
  return 0; // Enqueue successful
}

void *ptr_queue_dequeue(ptr_queue_t *queue, int timeout_ms) {
  struct timeval now;
  gettimeofday(&now, NULL);
  struct timespec ts;
  ts.tv_sec = now.tv_sec + timeout_ms / 1000;
  ts.tv_nsec = (now.tv_usec + (timeout_ms % 1000) * 1000) * 1000;
  ts.tv_sec += ts.tv_nsec / 1000000000;
  ts.tv_nsec %= 1000000000;

  pthread_mutex_lock(&queue->mutex);
  while (queue->size <= 0) {
    if (pthread_cond_timedwait(&queue->empty, &queue->mutex, &ts) ==
        ETIMEDOUT) {
      pthread_mutex_unlock(&queue->mutex);
      return NULL; // Timeout reached
    }
  }
  void *data = queue->queue[queue->front];
  queue->front = (queue->front + 1) % queue->max_size;
  queue->size--;
  pthread_cond_signal(&queue->full);
  pthread_mutex_unlock(&queue->mutex);
  return data; // Dequeue successful
}

void ptr_queue_cleanup(ptr_queue_t *queue) {
  free(queue->queue);
  pthread_mutex_destroy(&queue->mutex);
  pthread_cond_destroy(&queue->full);
  pthread_cond_destroy(&queue->empty);
}
