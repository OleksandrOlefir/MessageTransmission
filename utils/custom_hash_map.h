#ifndef CUSTOM_HASH_MAP_H
#define CUSTOM_HASH_MAP_H

#include <stddef.h>
#include <stdlib.h>
#include "message.h"

/* Smart pointer-like structure to manage memory manually */
typedef struct UniquePtr {
    void* ptr;  // Pointer to the managed memory
} UniquePtr;

/* CustomHashMapNode structure for the hash map's linked list (collision handling) */
typedef struct CustomHashMapNode {
    uint64_t key;        // Key (MessageId) for lookup
    Message value;       // Stored message value
    UniquePtr next;      // Pointer to the next node in the bucket
} CustomHashMapNode;

/* Array of buckets containing linked lists of nodes */
typedef struct BucketArray {
    UniquePtr* buckets;  // Array of pointers to nodes
    size_t size;         // Number of buckets
} BucketArray;

/* Main hash map structure */
typedef struct {
    UniquePtr buckets;   // Pointer to the bucket array
    size_t num_elements; // Total number of stored elements
} CustomHashMap;

/* Simple hash function using the key itself */
size_t hash_function(uint64_t key) {
    return key;  // Basic hash
}

/* Allocate and initialize a UniquePtr */
UniquePtr* make_unique_ptr(void* p) {
    UniquePtr* uptr = (UniquePtr*)malloc(sizeof(UniquePtr));
    uptr->ptr = p;
    return uptr;
}

/* Free a UniquePtr and its managed memory */
void free_unique_ptr(UniquePtr* uptr) {
    free(uptr->ptr);
    free(uptr);
}

/* Create a new hash map with an initial bucket size */
CustomHashMap* hash_map_create(size_t initial_size) {
    CustomHashMap* map = (CustomHashMap*)malloc(sizeof(CustomHashMap));
    map->num_elements = 0;
    BucketArray* ba = (BucketArray*)malloc(sizeof(BucketArray));
    ba->size = initial_size;
    ba->buckets = (UniquePtr*)calloc(initial_size, sizeof(UniquePtr)); // Zero-initialized
    map->buckets.ptr = ba;
    return map;
}

/* Destroy the hash map and free all allocated memory */
void hash_map_destroy(CustomHashMap* map) {
    BucketArray* ba = (BucketArray*)map->buckets.ptr;
    for (size_t i = 0; i < ba->size; i++) {
        CustomHashMapNode* current = (CustomHashMapNode*)ba->buckets[i].ptr;
        while (current) {
            CustomHashMapNode* next = (CustomHashMapNode*)current->next.ptr;
            free(current);  // Free each node in the bucket
            current = next;
        }
    }
    free(ba->buckets);  // Free the bucket array
    free(ba);           // Free the bucket array struct
    free(map);          // Free the hash map struct
}

/* Calculate the bucket index for a given key */
size_t get_bucket_index(CustomHashMap* map, uint64_t key) {
    BucketArray* ba = (BucketArray*)map->buckets.ptr;
    return hash_function(key) % ba->size;  // Modulo to fit within bucket count
}

/* Resize the hash map when load factor exceeds threshold */
void hash_map_resize(CustomHashMap* map) {
    BucketArray* old_ba = (BucketArray*)map->buckets.ptr;
    BucketArray* new_ba = (BucketArray*)malloc(sizeof(BucketArray));
    new_ba->size = old_ba->size * 2;  // Double the bucket count
    new_ba->buckets = (UniquePtr*)calloc(new_ba->size, sizeof(UniquePtr));
    UniquePtr new_buckets = {new_ba};

    // Rehash all existing elements into the new bucket array
    for (size_t i = 0; i < old_ba->size; i++) {
        CustomHashMapNode* current = (CustomHashMapNode*)old_ba->buckets[i].ptr;
        while (current) {
            CustomHashMapNode* next = (CustomHashMapNode*)current->next.ptr;
            size_t new_index = hash_function(current->key) % new_ba->size;
            current->next = new_ba->buckets[new_index];
            new_ba->buckets[new_index].ptr = current;
            current = next;
        }
    }
    free(old_ba->buckets);
    free(old_ba);
    map->buckets = new_buckets;
}

/* Insert a key-value pair into the hash map */
void hash_map_insert(CustomHashMap* map, uint64_t key, Message value) {
    size_t index = get_bucket_index(map, key);
    BucketArray* ba = (BucketArray*)map->buckets.ptr;
    CustomHashMapNode* current = (CustomHashMapNode*)ba->buckets[index].ptr;

    // Check for existing key (update if found)
    while (current) {
        if (current->key == key) {
            current->value = value;
            return;
        }
        current = (CustomHashMapNode*)current->next.ptr;
    }

    // Resize if load factor exceeds 0.75
    if ((map->num_elements + 1.0) / ba->size > 0.75) {
        hash_map_resize(map);
        ba = (BucketArray*)map->buckets.ptr;
        index = get_bucket_index(map, key);
    }

    // Create and insert new node
    CustomHashMapNode* new_node = (CustomHashMapNode*)malloc(sizeof(CustomHashMapNode));
    new_node->key = key;
    new_node->value = value;
    new_node->next = ba->buckets[index];
    ba->buckets[index].ptr = new_node;
    map->num_elements++;
}

/* Check if a key exists in the hash map */
int hash_map_contains(CustomHashMap* map, uint64_t key) {
    size_t index = get_bucket_index(map, key);
    BucketArray* ba = (BucketArray*)map->buckets.ptr;
    CustomHashMapNode* current = (CustomHashMapNode*)ba->buckets[index].ptr;
    while (current) {
        if (current->key == key) return 1;  // Found
        current = (CustomHashMapNode*)current->next.ptr;
    }
    return 0;  // Not found
}

/* Get the number of elements in the hash map */
size_t hash_map_size(CustomHashMap* map) {
    return map->num_elements;
}

#endif // CUSTOM_HASH_MAP_H