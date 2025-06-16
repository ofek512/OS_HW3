#include "segel.h"
#include "request.h"
#include "log.h"
#include "request_queue.h" // to add later

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
// TODO: HW3 — Initialize thread pool and request queue
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

void *worker_thread(void *arg)
{
    worker_unit *warg = (worker_unit*)arg;
    threads_stats t_stats = warg->stats;
    struct request_queue_t *queue = warg->queue;
    //server_log log = warg->log;

    while(1) {
        // Get a request from the queue
        struct request_t* request = queue_dequeue(queue);

        struct timeval dispatch;
        gettimeofday(&dispatch, NULL);

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
    getargs(&port, argc, argv);
    
    // Make request queue
    struct request_queue_t* queue = create_queue(QUEUE_SIZE);
    
    // make worker thread argument and threads
    pthread_t *threads = malloc(POOL_SIZE * sizeof(pthread_t));
    worker_unit *thread_args = malloc(POOL_SIZE * sizeof(worker_unit));
    
    // imaginary malloc fail check

    for (int i = 0; i < POOL_SIZE; ++i) {
        thread_args[i].stats = malloc(sizeof(struct Threads_stats));

        // set up thread arguments
        thread_args[i].stats->id = i;          // Thread ID
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

    // WE HAVE A THREAD POOL NOW!
    

    listenfd = Open_listenfd(port);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);

        // TODO: HW3 — Record the request arrival time here
        gettimeofday(&arrival, NULL);

        queue_enqueue(queue, connfd, arrival); // make sure the queue is not full

//        // DEMO PURPOSE ONLY:
//        // This is a dummy request handler that immediately processes
//        // the request in the main thread without concurrency.
//        // Replace this with logic to enqueue the connection and let
//        // a worker thread process it from the queue.
//
//        threads_stats t = malloc(sizeof(struct Threads_stats));
//        t->id = 0;             // Thread ID (placeholder)
//        t->stat_req = 0;       // Static request count
//        t->dynm_req = 0;       // Dynamic request count
//        t->total_req = 0;      // Total request count
//
//        struct timeval arrival, dispatch;
//        arrival.tv_sec = 0; arrival.tv_usec = 0;   // DEMO: dummy timestamps
//        dispatch.tv_sec = 0; dispatch.tv_usec = 0; // DEMO: dummy timestamps
//        // gettimeofday(&arrival, NULL);
//
//        // Call the request handler (immediate in main thread — DEMO ONLY)
//        requestHandle(connfd, arrival, dispatch, t, log);
//
//        free(t); // Cleanup
//        Close(connfd); // Close the connection
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

    // TODO: HW3 — Add cleanup code for thread pool and queue
}
