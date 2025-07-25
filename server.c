#include "segel.h"
#include "request.h"
#include "log.h"
#include "request_queue.h" // to add later
#include <stdio.h>
#include <stdlib.h>

//
// server.c: A very, very simple web server
//
// To run:
//  ./server <portnum (above 2000)>
//
// Repeatedly handles HTTP requests sent to this port number.
// Most of the work is done within routines written in request.c
//

// Parses command-line arguments
void getargs(int *port, int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    *port = atoi(argv[1]);
}
// This server currently handles all requests in the main thread.
// You must implement a thread pool (fixed number of worker threads)
// that process requests from a synchronized queue.

// define pool size and queue size
#define POOL_SIZE 4
#define QUEUE_SIZE 10


// Thread worker unit
typedef struct {
    threads_stats stats;
    struct request_queue_t *queue; // create the request queue
    server_log log;

} worker_unit;

struct timeval calculate_interval(struct timeval start, struct timeval end) {
    struct timeval temp = end;
    temp.tv_sec -= start.tv_sec;
    temp.tv_usec -= start.tv_usec;
    if (temp.tv_usec < 0) {
        temp.tv_sec -= 1;
        temp.tv_usec += 1000000;
    }
    return temp;
}

void *worker_thread(void *arg)
{
    worker_unit *warg = (worker_unit*)arg;
    threads_stats t_stats = warg->stats;
    struct request_queue_t *queue = warg->queue;

    while(1) {
        // Get a request from the queue
        struct request_t* request = queue_dequeue(queue);

        struct timeval dispatch;
        gettimeofday(&dispatch, NULL);
        dispatch = calculate_interval(request->arrival, dispatch);

        // Process the request
        requestHandle(request->connfd, request->arrival, dispatch, t_stats, warg->log);

        // Close connection
        Close(request->connfd);
        free(request);

    }
    return NULL;
}

int main(int argc, char *argv[])
{
    // Create the global server log

    int listenfd, connfd, port, clientlen;
    struct sockaddr_in clientaddr;
    struct timeval arrival;
    server_log log = create_log();
    if (!log) {
        perror("failed to init log");
        exit(1);
    }
    getargs(&port, argc, argv);

    // Make request queue
    struct request_queue_t* queue = create_queue(QUEUE_SIZE);
    
    // make worker thread argument and threads
    pthread_t *threads = malloc(POOL_SIZE * sizeof(pthread_t));
    worker_unit *thread_args = malloc(POOL_SIZE * sizeof(worker_unit));

    for (int i = 0; i < POOL_SIZE; ++i) {
        thread_args[i].stats = malloc(sizeof(struct Threads_stats));

        // set up thread arguments
        thread_args[i].stats->id = i + 1;          // Thread ID
        thread_args[i].stats->stat_req = 0;    // Static request count
        thread_args[i].stats->dynm_req = 0;    // Dynamic request count
        thread_args[i].stats->post_req = 0;    // POST request count
        thread_args[i].stats->total_req = 0;   // Total request count
        thread_args[i].queue = queue;          // Request queue
        thread_args[i].log = log;              // Server log

        if(pthread_create(&threads[i], NULL, worker_thread, &thread_args[i]) != 0) {
            perror("Failed to create thread");
            exit(1);
        }
    }


    listenfd = Open_listenfd(port);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);

        queue_enqueue(queue, connfd, arrival); // make sure the queue is not full
    }
    // Clean up the server log before exiting
    for (int i = 0; i < POOL_SIZE; ++i) {
        free(thread_args[i].stats);
        pthread_cancel(threads[i]);
        pthread_join(threads[i], NULL);
    }
    free(thread_args);
    free(threads);
    queue_destroy(queue); // Clean up the request queue TODO
    destroy_log(log);

}
