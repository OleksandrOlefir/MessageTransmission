#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <errno.h>
#include <stdlib.h>
#include "../utils/custom_convectors.h"
#include "../utils/custom_hash_map.h"
#include "../utils/custom_output.h"
#include "../utils/custom_queue.h"
#include "../utils/log_error.h"
#include "../utils/message.h"
#include "../utils/thread_utils.h"

/* Global variables for shared data and synchronization */
CustomHashMap* messageStore; // Stores received messages
CustomQueue* transmitQueue;  // Queue for messages to transmit
Mutex mtxStore, mtxQueue;    // Mutexes for thread safety
Cond cv;                     // Condition variable for signaling
int done = 0;                // Flag to terminate threads
ThreadPool* sendPool;        // Pool for async send tasks

/* Receiver thread function for UDP message reception */
void* receiverThread(void* arg) {
    int port = *(int*)arg;
    char name[16];
    snprintf(name, sizeof(name), "Receiver %d", port == 5000 ? 1 : 2);

    // Create UDP socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "%s socket creation failed", name);
        logError(buffer);
        return NULL;
    }
    fcntl(sock, F_SETFL, O_NONBLOCK);  // Set non-blocking mode

    // Bind socket to port
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        char buffer[256];
        snprintf(buffer, sizeof(buffer), "%s bind failed on port %d", name, port);
        logError(buffer);
        close(sock);
        return NULL;
    }

    // Use select to wait for incoming data
    fd_set read_fds;
    struct timeval tv;
    char buffer[sizeof(Message)];
    while (!done) {
        FD_ZERO(&read_fds);
        FD_SET(sock, &read_fds);
        tv.tv_sec = 0;
        tv.tv_usec = 10000;  // 10ms timeout to check done flag

        int ready = select(sock + 1, &read_fds, NULL, NULL, &tv);
        if (ready < 0) {
            char buffer[256];
            snprintf(buffer, sizeof(buffer), "%s select failed", name);
            logError(buffer);
            break;
        }
        if (ready == 0) {
            continue;  // Timeout, check done flag
        }

        if (FD_ISSET(sock, &read_fds)) {
            int bytes = recvfrom(sock, buffer, sizeof(Message), 0, NULL, NULL);
            if (bytes == sizeof(Message)) {
                Message msg;
                memcpy(&msg, buffer, sizeof(Message));
                msg.MessageSize = ntohs(msg.MessageSize);
                msg.MessageId = ntohll(msg.MessageId);
                msg.MessageData = ntohll(msg.MessageData);

                // Thread-safe insertion into messageStore
                mutex_lock(&mtxStore);
                if (!hash_map_contains(messageStore, msg.MessageId)) {
                    hash_map_insert(messageStore, msg.MessageId, msg);
                    char outBuffer[256];
                    snprintf(outBuffer, sizeof(outBuffer), "%s received: ID=%lu, Data=%lu\n", name, msg.MessageId, msg.MessageData);
                    print_out(outBuffer);

                    // Queue for transmission if MessageData == 10
                    if (msg.MessageData == 10) {
                        // Dynamically allocate the message to avoid dangling pointer
                        Message* msg_ptr = (Message*)malloc(sizeof(Message));
                        *msg_ptr = msg;
                        mutex_lock(&mtxQueue);
                        queue_push(transmitQueue, msg_ptr);  // Push the pointer to the allocated message
                        cond_signal(&cv);
                        mutex_unlock(&mtxQueue);
                    }
                } else {
                    char outBuffer[256];
                    snprintf(outBuffer, sizeof(outBuffer), "%s skipped duplicate ID=%lu\n", name, msg.MessageId);
                    print_out(outBuffer);
                }
                mutex_unlock(&mtxStore);
            } else if (bytes < 0 && errno != EAGAIN) {
                char buffer[256];
                snprintf(buffer, sizeof(buffer), "%s recvfrom failed", name);
                logError(buffer);
                break;
            }
        }
    }
    close(sock);
    return NULL;
}

/* Transmitter thread function for TCP sending */
void* transmitterThread(void* arg) {
    // Create TCP socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        logError("Transmitter socket creation failed");
        return NULL;
    }
    fcntl(sock, F_SETFL, O_NONBLOCK);

    // Connect to TCP receiver
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(6000);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    int retries = 5;
    while (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0 && retries > 0) {
        if (errno == EINPROGRESS) {
            fd_set write_fds;
            FD_ZERO(&write_fds);
            FD_SET(sock, &write_fds);
            struct timeval tv = {0, 100000}; // 100ms timeout
            select(sock + 1, NULL, &write_fds, NULL, &tv);
            if (FD_ISSET(sock, &write_fds)) {
                break;
            }
        }
        logError("TCP connect failed, retrying...");
        retries--;
        usleep(100000);
    }
    if (retries == 0) {
        logError("TCP connection failed after retries");
        close(sock);
        return NULL;
    }

    // Process transmit queue
    while (!done || !queue_empty(transmitQueue)) {
        mutex_lock(&mtxQueue);
        if (queue_empty(transmitQueue)) {
            cond_wait(&cv, &mtxQueue);  // Wait for new messages
            mutex_unlock(&mtxQueue);
            continue;
        }
        Message* msg_ptr = (Message*)queue_pop(transmitQueue);
        Message msg = *msg_ptr;  // Copy the message
        free(msg_ptr);           // Free the allocated memory
        mutex_unlock(&mtxQueue);

        // Create and add task to thread pool
        SendTask task;
        task.sock = dup(sock);  // Duplicate socket for thread safety
        task.msg = msg;
        pool_add_task(sendPool, task);
    }
    close(sock);
    return NULL;
}

int main() {
    // Initialize global data structures
    messageStore = hash_map_create(16);
    transmitQueue = queue_create();
    sendPool = pool_create(2);  // Use 2 worker threads for TCP sends
    mutex_init(&mtxStore);
    mutex_init(&mtxQueue);
    cond_init(&cv);

    // Start threads
    int port1 = 5000, port2 = 5001;
    Thread t1, t2, t3;
    thread_create(&t1, receiverThread, &port1);
    thread_create(&t2, receiverThread, &port2);
    thread_create(&t3, transmitterThread, NULL);

    // Run for 10 seconds
    sleep(10);
    done = 1;
    cond_signal(&cv);

    // Wait for threads to finish
    thread_join(t1);
    thread_join(t2);
    thread_join(t3);

    // Clean up any remaining messages in the transmit queue
    while (!queue_empty(transmitQueue)) {
        Message* msg_ptr = (Message*)queue_pop(transmitQueue);
        free(msg_ptr);
    }

    // Print termination message
    print_out("Program finished. Total unique messages: ");
    print_out_int((int)hash_map_size(messageStore));

    // Clean up resources
    hash_map_destroy(messageStore);
    queue_destroy(transmitQueue);
    pool_destroy(sendPool);
    mutex_destroy(&mtxStore);
    mutex_destroy(&mtxQueue);
    cond_destroy(&cv);
    return 0;
}
