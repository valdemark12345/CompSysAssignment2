#include "job_queue.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int job_queue_init(struct job_queue *job_queue, int capacity)
{
  job_queue->jobs = (struct job *)malloc(sizeof(struct job) * capacity);
  if (job_queue->jobs == NULL)
  {
    return -1;
  }

  pthread_mutex_init(&job_queue->lock, NULL);
  pthread_cond_init(&job_queue->empty_cond, NULL);
  pthread_cond_init(&job_queue->full_cond, NULL);
  pthread_cond_init(&job_queue->done_cond, NULL);

  job_queue->capacity = capacity;
  job_queue->back = 0;
  job_queue->front = 0;
  job_queue->size = 0;
  job_queue->destroyed = 0;
  job_queue->active_workers = 0;

  return 0;
}

int job_queue_destroy(struct job_queue *job_queue)
{
  pthread_mutex_lock(&job_queue->lock);

  // Wait until queue is completely empty
  while (job_queue->size > 0)
  {
    pthread_cond_wait(&job_queue->full_cond, &job_queue->lock);
  }

  // Mark as destroyed
  job_queue->destroyed = 1;

  // Wake up all waiting threads
  pthread_cond_broadcast(&job_queue->empty_cond);
  pthread_cond_broadcast(&job_queue->full_cond);

  // Wait until all active workers have exited
  while (job_queue->active_workers > 0)
  {
    pthread_cond_wait(&job_queue->done_cond, &job_queue->lock);
  }

  pthread_mutex_unlock(&job_queue->lock);

  // Now it's safe to destroy everything
  pthread_mutex_destroy(&job_queue->lock);
  pthread_cond_destroy(&job_queue->empty_cond);
  pthread_cond_destroy(&job_queue->full_cond);
  pthread_cond_destroy(&job_queue->done_cond);

  free(job_queue->jobs);

  return 0;
}

int job_queue_push(struct job_queue *job_queue, void *data)
{
  pthread_mutex_lock(&job_queue->lock);

  // Check if destroyed first
  if (job_queue->destroyed)
  {
    pthread_mutex_unlock(&job_queue->lock);
    return -1;
  }

  // Wait while the queue is full
  while (job_queue->size == job_queue->capacity)
  {
    pthread_cond_wait(&job_queue->full_cond, &job_queue->lock);
  }

  // Push the job
  job_queue->jobs[job_queue->back].arg = data;
  job_queue->back = (job_queue->back + 1) % job_queue->capacity;
  job_queue->size++;

  // Signal that queue is not empty anymore
  pthread_cond_signal(&job_queue->empty_cond);
  pthread_mutex_unlock(&job_queue->lock);

  return 0;
}

int job_queue_pop(struct job_queue *job_queue, void **data)
{
  pthread_mutex_lock(&job_queue->lock);
  job_queue->active_workers++;
  // Wait while empty AND not destroyed
  while (job_queue->size == 0 && !job_queue->destroyed)
  {
    pthread_cond_wait(&job_queue->empty_cond, &job_queue->lock);
  }

  // If destroyed and empty, return error
  if (job_queue->destroyed && job_queue->size == 0)
  {
    job_queue->active_workers--;

    if (job_queue->active_workers == 0)
    {
      pthread_cond_signal(&job_queue->done_cond);
    }
    pthread_mutex_unlock(&job_queue->lock);
    return -1;
  }

  // Pop the job
  *data = job_queue->jobs[job_queue->front].arg;
  job_queue->front = (job_queue->front + 1) % job_queue->capacity;
  job_queue->size--;

  // Signal that queue has space now and that it's done
  pthread_cond_signal(&job_queue->full_cond);
  job_queue->active_workers--;

  if (job_queue->destroyed && job_queue->active_workers == 0)
  {
    pthread_cond_signal(&job_queue->done_cond);
  }

  pthread_mutex_unlock(&job_queue->lock);

  return 0;
}
