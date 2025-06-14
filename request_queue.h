//
// Created by ofek5 on 2025-06-13.
//

#ifndef OS_HW3_REQUEST_QUEUE_H
#define OS_HW3_REQUEST_QUEUE_H


typedef struct request_t {
    int connfd;
    struct timeval arrival;  // Time the request arrived
    struct request_t *next; // Pointer to the next request in the queue
};


typedef struct request_queue_t{
    struct request_t *head;        // pointer to the oldest
    struct request_t *tail;       // pointer to the newest
    int size;                    // current size of the queue
    int capacity;               // maximum capacity of the queue
    pthread_mutex_t mutex;     // Mutex for thread-safe access
    pthread_cond_t not_empty; // Condition variable for when the queue is not empty
    pthread_cond_t not_full; // Condition variable for when the queue is not full
};

void queue_destroy(struct request_queue_t *queue);

int queue_enqueue(struct request_queue_t *queue, int connfd, struct timeval arrival);

struct request_t* queue_dequeue(struct request_queue_t *queue);

struct request_queue_t* create_queue(int capacity);

#endif //OS_HW3_REQUEST_QUEUE_H
