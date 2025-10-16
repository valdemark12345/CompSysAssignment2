#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "job_queue.h"

// Simple test function
void test_function(void *arg) {
    int *value = (int *)arg;
    printf("Executing job with value: %d\n", *value);
}

int main(void) {
    struct job_queue q;
    
    // Initialize queue with capacity 5
    if (job_queue_init(&q, 5) != 0) {
        printf("Failed to initialize queue!\n");
        return 1;
    }
    printf("Queue initialized successfully\n");
    
    // Push some jobs
    int values[] = {1, 2, 3, 4, 5};
    
    for (int i = 0; i < 5; i++) {
        struct job j;
        j.function = test_function; 
        j.arg = &values[i];
        
        if (job_queue_push(&q, &j) != 0) {
            printf("Failed to push job %d\n", i);
        } else {
            printf("Pushed job %d\n", i+1);
        }
    }
    
    // Pop and execute jobs
    void *data;
    for (int i = 0; i < 5; i++) {
        if (job_queue_pop(&q, &data) == 0) {
            struct job *j = (struct job *)data;
            // Cast the function pointer properly
            void (*func)(void *) = (void (*)(void *))j->function;
            func(j->arg);
        } else {
            printf("Failed to pop job %d!\n", i);
        }
    }
    
    // Destroy queue
    if (job_queue_destroy(&q) != 0) {
        printf("Failed to destroy queue!\n");
        return 1;
    }
    printf("Queue destroyed successfully\n");
    
    return 0;
}
