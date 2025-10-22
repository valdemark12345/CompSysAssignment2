#include "job_queue.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

//Setting up the circular queue. 
int job_queue_init(struct job_queue *job_queue, int capacity)
{
  // Malloc space for queue of jobs
  job_queue->jobs = (struct job *)malloc(sizeof(struct job) * capacity);
  if (job_queue->jobs == NULL)
  {
    return -1;
  }

  //Initializes the mutex and condition variables
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

//Shut down the queue.
int job_queue_destroy(struct job_queue *jq)
{
  pthread_mutex_lock(&jq->lock);

  // Wait until queue is empty
  while (jq->size > 0) {
    pthread_cond_wait(&jq->full_cond, &jq->lock);
  }

  // Tell consumers no more work will arrive; wake any waiters
  jq->destroyed = 1;
  pthread_cond_broadcast(&jq->empty_cond);
  pthread_cond_broadcast(&jq->full_cond);

  // Wait until all workers have stopped using the queue
  while (jq->active_workers > 0) {
    pthread_cond_wait(&jq->done_cond, &jq->lock);
  }

  pthread_mutex_unlock(&jq->lock);

  //Release buffer memory
  free(jq->jobs);
  jq->jobs = NULL;
  return 0;
}

//enqueue the jobs
int job_queue_push(struct job_queue *job_queue, void *data)
{
  pthread_mutex_lock(&job_queue->lock);

  //Check if destroyed first
  if (job_queue->destroyed)
  {
    pthread_mutex_unlock(&job_queue->lock);
    return -1;
  }

  //Wait while the queue is full
  while (job_queue->size == job_queue->capacity)
  {
    //if queue is full -> Lock
    pthread_cond_wait(&job_queue->full_cond, &job_queue->lock);
  }

  //Push the job
  job_queue->jobs[job_queue->back].arg = data;
  job_queue->back = (job_queue->back + 1) % job_queue->capacity;
  job_queue->size++;

  //Change the queue is not empty anymore
  pthread_cond_signal(&job_queue->empty_cond);
  pthread_mutex_unlock(&job_queue->lock);

  return 0;
}

int job_queue_pop(struct job_queue *job_queue, void **data)
{
  //Thread-local variable to track if this thread has an active job
  static __thread int has_active_job = 0;

  pthread_mutex_lock(&job_queue->lock);

  //If this thread had an active job, it's now complete
  if (has_active_job)
  {
    //Decrement activeworkers and the workers field has_active_job
    job_queue->active_workers--;
    has_active_job = 0;
    
    //If the job_queue is destoyed and there is no more active workers -> signal done.
    if (job_queue->destroyed && job_queue->active_workers == 0)
    {
      pthread_cond_signal(&job_queue->done_cond);
    }
  }

  // Wait while empty AND not destroyed
  while (job_queue->size == 0 && !job_queue->destroyed)
  {
    pthread_cond_wait(&job_queue->empty_cond, &job_queue->lock);
  }

  // If destroyed and empty, return error
  if (job_queue->destroyed && job_queue->size == 0)
  {
    pthread_mutex_unlock(&job_queue->lock);
    return -1;
  }

  // Pop the job
  *data = job_queue->jobs[job_queue->front].arg;
  job_queue->front = (job_queue->front + 1) % job_queue->capacity;
  job_queue->size--;

  // This thread now has an active job
  job_queue->active_workers++;
  has_active_job = 1;

  // Signal that queue has space now
  pthread_cond_signal(&job_queue->full_cond);

  pthread_mutex_unlock(&job_queue->lock);

  return 0;
}
