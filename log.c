#include <stdlib.h>
#include <string.h>
#include "log.h"

// Opaque struct definition
struct Log_entry {
    char* data;
    int data_len;
    struct Log_entry* next;
};

struct Server_log {
    struct Log_entry* head; // Pointer to the first log entry
    struct Log_entry* tail; // Pointer to the last log entry
    int size;
    pthread_mutex_t mutex; // Mutex for thread-safe access
    pthread_cond_t read_allowed;
    pthread_cond_t write_allowed;
    int readers, writers, waiting_writers;

    // TODO: Implement internal log storage (e.g., dynamic buffer, linked list, etc.)
};

// Creates a new server log instance (stub)
server_log create_log() {
    server_log result = (server_log)malloc(sizeof(struct Server_log));
    result->head = NULL;
    result->tail = NULL;
    result->size = 0;
    result->readers = 0;
    result->writers = 0;
    result->waiting_writers = 0;
    pthread_mutex_init(&(result->mutex), NULL);
    pthread_cond_init(&result->read_allowed, NULL);
    pthread_cond_init(&result->write_allowed, NULL);
    return result;
}

// Destroys and frees the log (stub)
void destroy_log(server_log log) {
//    pthread_mutex_lock(&(log->mutex)); // Check this
    if (!log) return;
    struct Log_entry* current = log->head;
    while(current) {
        struct Log_entry* next = current->next;
        free(current->data); // Free the data string
        free(current); // Free the log entry
        current = next;
    }
//    pthread_mutex_unlock(&(log->mutex));
    pthread_mutex_destroy(&(log->mutex));
    pthread_cond_destroy(&log->read_allowed);
    pthread_cond_destroy(&log->write_allowed);
    free(log);
}

void reader_lock(server_log log) {
    pthread_mutex_lock(&(log->mutex));
    while (log->waiting_writers > 0 || log->writers > 0) {
        pthread_cond_wait(&(log->read_allowed), &(log->mutex));
    }
    log->readers++;
    pthread_mutex_unlock(&(log->mutex));
}

void reader_unlock(server_log log) {
    pthread_mutex_lock(&(log->mutex));
    log->readers--;
    if (log->readers == 0) {
        pthread_cond_signal(&(log->write_allowed));
    }
    pthread_mutex_unlock(&(log->mutex));
}

void writer_lock(server_log log) {
    pthread_mutex_lock(&(log->mutex));
    log->waiting_writers++;
    while(log->readers + log->writers > 0) {
        pthread_cond_wait(&(log->write_allowed), &(log->mutex));
    }
    log->waiting_writers--;
    log->writers++;
    pthread_mutex_unlock(&(log->mutex));
}

void writer_unlock(server_log log) {
    pthread_mutex_lock(&(log->mutex));
    log->writers--;
    if (log->writers == 0) {
        pthread_cond_signal(&(log->write_allowed));
        pthread_cond_broadcast(&(log->read_allowed));
    }
    pthread_mutex_unlock(&(log->mutex));
}

// Returns dummy log content as string (stub)
int get_log(server_log log, char** dst) {
    if (!log) return 0;
    int len = 1;
    reader_lock(log);
    struct Log_entry* current = log->head;
    for(int i = 0; i < log->size; i++) {
        len += current->data_len; // Maybe need to check '\n'
        current = current->next;
    }
    char* buf = (char*)malloc(len);
    // Check malloc
    if(!buf) {
        perror("Malloc failed");
        reader_unlock(log);
        return 0;
    }
    current = log->head;
    for (int i = 0; i < log->size; i++) {
        strcat(buf, current->data);
        current = current->next; // Maybe '\n'
    }
    reader_unlock(log);
    if(*dst) {
        strcpy(*dst, buf);
    }
    free(buf);
    return len;
//    const char* dummy = "Log is not implemented.\n";
//    int len = strlen(dummy);
//    *dst = (char*)malloc(len + 1); // Allocate for caller
//    if (*dst != NULL) {
//        strcpy(*dst, dummy);
//    }
//    return len;
}

// Appends a new entry to the log (no-op stub)
void add_to_log(server_log log, const char* data, int data_len) {
    if (!data || data_len <= 0 || !log) {
        perror("invalid arguments");
        return;
    }
    struct Log_entry* to_append = (struct Log_entry*)malloc(sizeof(struct Log_entry));
    // Check malloc
    if (!to_append) {
        perror("Malloc failed");
        return;
    }
    to_append->data = (char*)malloc(data_len);
    memcpy(to_append->data, data, data_len);
    to_append->data_len = data_len;
    to_append->next = NULL;

    writer_lock(log);
    if(!log->tail) {
        // Was empty
        log->head = log->tail = to_append;
    } else {
        log->tail->next = to_append;
        log->tail = to_append;
    }
    log->size++;
    writer_unlock(log);
}
