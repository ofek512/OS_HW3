//
// Created by ofek5 on 2025-06-13.
//

#include "request_queue.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

void queue_destroy(struct request_queue_t *queue) {
    if (!queue) return;

    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->not_empty);
    pthread_cond_destroy(&queue->not_full);

    struct request_t *current = queue->head;
    while (current != queue->tail) {
        struct request_t *next = current->next;
        free(current);
        current = next;
    }
    free(queue);
}

struct request_t* queue_dequeue(struct request_queue_t *queue) {
    pthread_mutex_lock(&queue->mutex);

    // Wait until the queue is not empty
    while (queue->size == 0) {
        pthread_cond_wait(&queue->not_empty, &queue->mutex); // wait for a request to be available
    }

    // Get request
    struct request_t* request = queue->head;
    // check if queue is empty before going to next
    if (queue->size == 1) {
        queue->head = NULL;
        queue->tail = NULL;
    } else {
        queue->head = request->next; // Move head to the next request
        //check if was full, send signal if so
        if (queue->size == queue->capacity) {
            pthread_cond_signal(&queue->not_full); // signal that the queue is not full
        }
    }
    queue->size--;
    pthread_mutex_unlock(&queue->mutex);

    return request; // Return the dequeued request
}

int queue_enqueue(struct request_queue_t *queue, int connfd, struct timeval arrival) {
    pthread_mutex_lock(&queue->mutex);

    // Wait until the queue is not full
    while (queue->size == queue->capacity) {
        pthread_cond_wait(&queue->not_full, &queue->mutex); // wait for space to be available
    }

    struct request_t current_request = {connfd, arrival, NULL};
    struct request_t *new_request = malloc(sizeof(struct request_t));
    new_request = &current_request; // Copy the current request into the new request
    if(queue->size == 0) {
        queue -> head = new_request;
        queue -> tail = new_request; // If queue was empty, set both head and tail to the new request
        pthread_cond_signal(&queue->not_empty); // signal that the queue is not empty
    }
    else {
        queue->tail->next = new_request; // Link the new request to the end of the queue
        queue->tail = new_request; // Update the tail to the new request
    }
    queue->size++; // Increment the size of the queue
    pthread_mutex_unlock(&queue->mutex);
    return 0;
}