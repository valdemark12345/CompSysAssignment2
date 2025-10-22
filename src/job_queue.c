#include "job_queue.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

//Setting up the circular queue.
int job_queue_init(struct job_queue *job_queue, int capacity)
{
  //Malloc space for jobs
  job_queue->jobs = (struct job *)malloc(sizeof(struct job) * capacity);
  if (job_queue->jobs == NULL)
  {
    return -1;
  }

  // Initializes the mutex and condition variables
  pthread_mutex_init(&job_queue->lock, NULL);
  pthread_cond_init(&job_queue->empty_cond, NULL);
  pthread_cond_init(&job_queue->full_cond, NULL);
  pthread_cond_init(&job_queue->done_cond, NULL);

  //Reset queue state
  job_queue->capacity = capacity;
  job_queue->back = 0;
  job_queue->front = 0;
  job_queue->size = 0;
  job_queue->destroyed = 0;
  job_queue->active_workers = 0;

  return 0;
}

//Shut down the queue
int job_queue_destroy(struct job_queue *jq)
{
  //Lock the queue so only one thread touches state
  pthread_mutex_lock(&jq->lock);

  //Block until queue is empty
  while (jq->size > 0) {
    pthread_cond_wait(&jq->full_cond, &jq->lock);
  }

  //Set shutdown flag and wake all threat
  jq->destroyed = 1;
  pthread_cond_broadcast(&jq->empty_cond);
  pthread_cond_broadcast(&jq->full_cond);

  //Wait until all jobs are finished
  while (jq->active_workers > 0) {
    pthread_cond_wait(&jq->done_cond, &jq->lock);
  }

  //Unlock the queue
  pthread_mutex_unlock(&jq->lock);

  // Release buffer memory
  free(jq->jobs);
  jq->jobs = NULL;
  return 0;
}

//Enqueue the jobs
int job_queue_push(struct job_queue *job_queue, void *data)
{
  //Lock to protect shared queue so only one threat can add a job
  pthread_mutex_lock(&job_queue->lock);

  // Abort if job_queue is set to be destroyed
  if (job_queue->destroyed)
  {
    pthread_mutex_unlock(&job_queue->lock);
    return -1;
  }

  //Wait while queue is full
  while (job_queue->size == job_queue->capacity)
  {
    pthread_cond_wait(&job_queue->full_cond, &job_queue->lock);
  }

  //Add new job and move tail pointer
  job_queue->jobs[job_queue->back].arg = data;
  job_queue->back = (job_queue->back + 1) % job_queue->capacity;
  job_queue->size++;

  //Signal a worker that a job is available
  pthread_cond_signal(&job_queue->empty_cond);
  pthread_mutex_unlock(&job_queue->lock);

  return 0;
}

int job_queue_pop(struct job_queue *job_queue, void **data)
{
  //Thread-local flag to track active job
  static __thread int has_active_job = 0;

  //Lock to prevent multiple workers taking same job
  pthread_mutex_lock(&job_queue->lock);

  // If returning after a job -> signal the job is completed, and decrement activeworker count
  if (has_active_job)
  {
    job_queue->active_workers--;
    has_active_job = 0;
    
    //If shutdown and last worker -> flag to destroy
    if (job_queue->destroyed && job_queue->active_workers == 0)
    {
      pthread_cond_signal(&job_queue->done_cond);
    }
  }

  //Wait for work while not shutting down
  while (job_queue->size == 0 && !job_queue->destroyed)
  {
    pthread_cond_wait(&job_queue->empty_cond, &job_queue->lock);
  }

  //If shutting down and nothing left -> exit
  if (job_queue->destroyed && job_queue->size == 0)
  {
    pthread_mutex_unlock(&job_queue->lock);
    return -1;
  }

  //Read item at front and advance
  *data = job_queue->jobs[job_queue->front].arg;
  job_queue->front = (job_queue->front + 1) % job_queue->capacity;
  job_queue->size--;

  //Mark this thread as active worker
  job_queue->active_workers++;
  has_active_job = 1;

  //Notify a threat that space exists
  pthread_cond_signal(&job_queue->full_cond);

  //Unlock the queue so other threads can continue
  pthread_mutex_unlock(&job_queue->lock);

  return 0;
}

