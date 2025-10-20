#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "job_queue.h"


int job_queue_init(struct job_queue *job_queue, int capacity) {
  //Inisiates the que
  job_queue->jobs = (struct job *)malloc(sizeof(struct job) * capacity);
  if (job_queue->jobs == NULL) {return -1;}

  //Inisiates the mutex and condition 
  pthread_mutex_init(&job_queue->lock, NULL);
  pthread_cond_init(&job_queue->empty_cond, NULL);
  pthread_cond_init(&job_queue->full_cond, NULL);
  
  //Sets the capacity, back and front 
  job_queue->capacity = capacity;
  job_queue->back = 0;
  job_queue->front = 0;
  job_queue->size = 0;

  return 0;
}

int job_queue_destroy(struct job_queue *job_queue) {
  
  //Locks the mutex 
  pthread_mutex_lock(&job_queue->lock);
  
  //Checks if the jobqueue is empty 
  while (job_queue->size > 0) {
    pthread_cond_wait(&job_queue->full_cond, &job_queue->lock); //Makes it wait until size is 0 
  }
  //If the pop_wait is 1, then sent signal to pop to return -1 
  if (job_queue->pop_wait == 1){
    job_queue->destory_wait = 1;
    pthread_cond_signal(&job_queue->empty_cond);
  }

  //Destroys Mutex and conditions 
  pthread_mutex_destroy(&job_queue->lock);
  pthread_cond_destroy(&job_queue->empty_cond);
  pthread_cond_destroy(&job_queue->full_cond);

  //Frees job queue array 
  free(job_queue->jobs);

  pthread_mutex_unlock(&job_queue->lock);

  return 0;
}

int job_queue_push(struct job_queue *job_queue, void *data) {

  //Locks the mutex 
  pthread_mutex_lock(&job_queue->lock);
  
  //checks if job queue is full 
  while(job_queue->size == job_queue->capacity) {
    pthread_cond_wait(&job_queue->full_cond, &job_queue->lock); //If full, waits until not full
  }

  //Specifics for how front and back of queue is determines 
  job_queue->jobs[job_queue->back] = *(struct job*) data;
  job_queue->back = (job_queue->back + 1) % job_queue->capacity;
  job_queue->size++;

  //Sebt signal that job queue is not empty anymore 
  pthread_cond_signal(&job_queue->empty_cond);


  pthread_mutex_unlock(&job_queue->lock);
  return 0; 
}

int job_queue_pop(struct job_queue *job_queue, void **data) {

  //Locks the mutex 
  pthread_mutex_lock(&job_queue->lock);

  //Checks if size is 0
  while(job_queue->size == 0){
    pthread_cond_wait(&job_queue->empty_cond, &job_queue->lock); //Makes it wait until size is not zero
    job_queue->pop_wait = 1;
  };
  //Sets pop_wait to not active 
  job_queue->pop_wait = 0;

  //If a destroy queue is waiting return while pop was waiting -1 
  if (job_queue->destory_wait == 1) 
  {
    pthread_mutex_unlock(&job_queue->lock);
    return -1; 
  }
  //Saves the pointer of data to the row which is poped. 
  //Specifics for how the front and back works for the queue
  *data = &job_queue->jobs[job_queue->front];
  job_queue->front = (job_queue->front + 1) % job_queue->capacity;
  job_queue->size--;

  //Sents a signal that the queue is not full
  pthread_cond_signal(&job_queue->full_cond);

  pthread_mutex_unlock(&job_queue->lock);

  return 0;
}
