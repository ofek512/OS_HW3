//
// Created by ofek5 on 2025-06-13.
//

#include "request_queue.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>


struct request_queue_t* create_queue(int capacity) {
    struct request_queue_t* queue = malloc(sizeof(struct request_queue_t));
    if (!queue) return NULL;

    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
    queue->capacity = capacity;

    pthread_mutex_init(&(queue->mutex), NULL);
    pthread_cond_init(&queue->not_empty, NULL);
    pthread_cond_init(&queue->not_full, NULL);

    return queue;
}

void queue_destroy(struct request_queue_t *queue) {
    if (!queue) return;

    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->not_empty);
    pthread_cond_destroy(&queue->not_full);

    struct request_t *current = queue->head;
    while (current != NULL) {
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
        pthread_cond_wait(&queue->not_empty, &queue->mutex);
    }

    // Get request
    struct request_t* request = queue->head;

    // Update queue pointers
    if (queue->size == 1) {
        queue->head = NULL;
        queue->tail = NULL;
    } else {
        queue->head = request->next;
    }

    queue->size--;

    // Signal if queue was full before dequeuing
    if (queue->size == queue->capacity - 1) {
        pthread_cond_signal(&queue->not_full);
    }

    pthread_mutex_unlock(&queue->mutex);
    return request;
}

int queue_enqueue(struct request_queue_t *queue, int connfd, struct timeval arrival) {
    pthread_mutex_lock(&queue->mutex);

    // Wait until the queue is not full
    while (queue->size == queue->capacity) {
        pthread_cond_wait(&queue->not_full, &queue->mutex);
    }

    // Allocate and initialize new request
    struct request_t *new_request = malloc(sizeof(struct request_t));
    if (!new_request) {
        pthread_mutex_unlock(&queue->mutex);
        return -1; // Memory allocation failed
    }

    // Copy data to new request
    new_request->connfd = connfd;
    new_request->arrival = arrival;
    new_request->next = NULL;

    // Add to queue
    if (queue->size == 0) {
        queue->head = new_request;
        queue->tail = new_request;
        pthread_cond_signal(&queue->not_empty);
    } else {
        queue->tail->next = new_request;
        queue->tail = new_request;
    }

    queue->size++;
    pthread_mutex_unlock(&queue->mutex);
    return 0;
}