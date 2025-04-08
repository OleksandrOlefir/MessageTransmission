#ifndef THREAD_UTILS_H
#define THREAD_UTILS_H

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include "message.h"
#include "custom_convectors.h"
#include "custom_queue.h"
#include "custom_output.h"
#include "log_error.h"

/* Type aliases for POSIX thread primitives */
typedef pthread_t Thread;
typedef pthread_mutex_t Mutex;
typedef pthread_cond_t Cond;

/* Create a new thread */
void thread_create(Thread* thread, void* (*func)(void*), void* arg) {
    pthread_create(thread, NULL, func, arg);
}

/* Detach a thread so it cleans up automatically when it finishes */
void thread_detach(Thread thread) {
    pthread_detach(thread);
}

/* Wait for a thread to complete */
void thread_join(Thread thread) {
    pthread_join(thread, NULL);
}

/* Initialize a mutex */
void mutex_init(Mutex* mutex) {
    pthread_mutex_init(mutex, NULL);
}

/* Lock a mutex */
void mutex_lock(Mutex* mutex) {
    pthread_mutex_lock(mutex);
}

/* Unlock a mutex */
void mutex_unlock(Mutex* mutex) {
    pthread_mutex_unlock(mutex);
}

/* Destroy a mutex */
void mutex_destroy(Mutex* mutex) {
    pthread_mutex_destroy(mutex);
}

/* Initialize a condition variable */
void cond_init(Cond* cond) {
    pthread_cond_init(cond, NULL);
}

/* Wait on a condition variable with a mutex */
void cond_wait(Cond* cond, Mutex* mutex) {
    pthread_cond_wait(cond, mutex);
}

/* Signal a condition variable */
void cond_signal(Cond* cond) {
    pthread_cond_signal(cond);
}

/* Broadcast to all waiting threads on a condition variable */
void cond_broadcast(Cond* cond) {
    pthread_cond_broadcast(cond);
}

/* Destroy a condition variable */
void cond_destroy(Cond* cond) {
    pthread_cond_destroy(cond);
}

/* Structure for asynchronous send tasks */
typedef struct {
    int sock;    // Socket file descriptor for sending
    Message msg; // Message to send
} SendTask;

/* Thread pool for managing async send tasks */
typedef struct {
    CustomQueue* tasks;    // Queue of SendTask structs
    Thread* workers;       // Array of worker threads
    size_t num_workers;    // Number of worker threads
    Mutex mutex;           // Mutex for thread-safe task access
    Cond cond;             // Condition variable for task availability
    int shutdown;          // Flag to signal shutdown
} ThreadPool;

/* Worker thread function for the thread pool */
void* asyncSendWorker(void* arg) {
    ThreadPool* pool = (ThreadPool*)arg;
    while (1) {
        mutex_lock(&pool->mutex);
        while (queue_empty(pool->tasks) && !pool->shutdown) {
            cond_wait(&pool->cond, &pool->mutex);
        }
        if (pool->shutdown && queue_empty(pool->tasks)) {
            mutex_unlock(&pool->mutex);
            break;
        }
        SendTask* task = (SendTask*)queue_pop(pool->tasks);
        mutex_unlock(&pool->mutex);

        // Perform the send operation
        int sock = task->sock;
        Message msg = task->msg;
        Message netMsg = msg;
        netMsg.MessageSize = htons(msg.MessageSize);
        netMsg.MessageId = htonll(msg.MessageId);
        netMsg.MessageData = htonll(msg.MessageData);

        ssize_t sent = 0;
        while (sent < sizeof(Message)) {
            ssize_t result = send(sock, (char*)&netMsg + sent, sizeof(Message) - sent, 0);
            if (result < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // Use select to wait for the socket to be writable
                    fd_set write_fds;
                    FD_ZERO(&write_fds);
                    FD_SET(sock, &write_fds);
                    struct timeval tv = {0, 10000}; // 10ms timeout
                    select(sock + 1, NULL, &write_fds, NULL, &tv);
                    continue;
                }
                char buffer[256];
                snprintf(buffer, sizeof(buffer), "Async send failed for ID=%lu", msg.MessageId);
                logError(buffer);
                break;
            }
            sent += result;
        }
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "Transmitted: ID=%lu\n", msg.MessageId);
        print_out(buffer);
        close(sock);  // Close duplicated socket
        free(task);   // Free the task memory
    }
    return NULL;
}

/* Create a new thread pool with a specified number of workers */
ThreadPool* pool_create(size_t num_workers) {
    ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));
    pool->tasks = queue_create();
    pool->num_workers = num_workers;
    pool->workers = (Thread*)malloc(sizeof(Thread) * num_workers);
    pool->shutdown = 0;
    mutex_init(&pool->mutex);
    cond_init(&pool->cond);

    // Start worker threads
    for (size_t i = 0; i < num_workers; i++) {
        thread_create(&pool->workers[i], asyncSendWorker, pool);
    }
    return pool;
}

/* Destroy the thread pool and free resources */
void pool_destroy(ThreadPool* pool) {
    mutex_lock(&pool->mutex);
    pool->shutdown = 1;
    cond_broadcast(&pool->cond);
    mutex_unlock(&pool->mutex);

    // Wait for workers to finish
    for (size_t i = 0; i < pool->num_workers; i++) {
        thread_join(pool->workers[i]);
    }

    // Free remaining tasks in the queue
    while (!queue_empty(pool->tasks)) {
        SendTask* task = (SendTask*)queue_pop(pool->tasks);
        close(task->sock);  // Close the socket
        free(task);         // Free the task memory
    }
    queue_destroy(pool->tasks);
    free(pool->workers);
    mutex_destroy(&pool->mutex);
    cond_destroy(&pool->cond);
    free(pool);
}

/* Add a task to the thread pool */
void pool_add_task(ThreadPool* pool, SendTask task) {
    // Allocate memory for the task since the queue only stores pointers
    SendTask* task_ptr = (SendTask*)malloc(sizeof(SendTask));
    *task_ptr = task;
    mutex_lock(&pool->mutex);
    queue_push(pool->tasks, task_ptr);
    cond_signal(&pool->cond);
    mutex_unlock(&pool->mutex);
}

#endif // THREAD_UTILS_H