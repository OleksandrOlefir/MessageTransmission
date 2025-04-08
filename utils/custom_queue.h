#ifndef CUSTOM_QUEUE_H
#define CUSTOM_QUEUE_H

#include "message.h"
#include <stdlib.h>

/* CustomQueueNode structure for the queue's linked list */
typedef struct CustomQueueNode {
    void* data;                   // Generic pointer to store any data
    struct CustomQueueNode* next; // Pointer to the next node
} CustomQueueNode;

/* Queue structure implemented as a linked list */
typedef struct {
    CustomQueueNode* front;       // Pointer to the front node
    CustomQueueNode* rear;        // Pointer to the rear node
    unsigned int size; // Number of elements in the queue
} CustomQueue;

/* Create a new empty queue */
CustomQueue* queue_create() {
    CustomQueue* q = (CustomQueue*)malloc(sizeof(CustomQueue));
    q->front = NULL;
    q->rear = NULL;
    q->size = 0;
    return q;
}

/* Destroy the queue and free all nodes (does not free the data) */
void queue_destroy(CustomQueue* q) {
    while (q->front) {
        CustomQueueNode* temp = q->front;
        q->front = q->front->next;
        free(temp);  // Free each node (but not the data it points to)
    }
    free(q);  // Free the queue struct
}

/* Add a value to the rear of the queue */
void queue_push(CustomQueue* q, void* value) {
    CustomQueueNode* newCustomQueueNode = (CustomQueueNode*)malloc(sizeof(CustomQueueNode));
    newCustomQueueNode->data = value;
    newCustomQueueNode->next = NULL;
    if (!q->front) {
        q->front = q->rear = newCustomQueueNode;  // First element
    } else {
        q->rear->next = newCustomQueueNode;       // Append to rear
        q->rear = newCustomQueueNode;
    }
    q->size++;
}

/* Remove and return the value from the front of the queue */
void* queue_pop(CustomQueue* q) {
    if (!q->front) {
        return NULL;  // Return NULL if queue is empty
    }
    CustomQueueNode* temp = q->front;
    void* value = temp->data;
    q->front = q->front->next;
    free(temp);  // Free the removed node
    q->size--;
    if (!q->front) q->rear = NULL;  // Reset rear if queue becomes empty
    return value;
}

/* Check if the queue is empty */
int queue_empty(CustomQueue* q) {
    return q->size == 0;
}

#endif // CUSTOM_QUEUE_H