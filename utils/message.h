#ifndef MESSAGE_H
#define MESSAGE_H

#include <stdint.h>

/* Defines the structure for messages used across the system */
typedef struct {
    uint16_t MessageSize;  // Size of the message in bytes
    uint8_t MessageType;   // Type identifier for the message
    uint64_t MessageId;    // Unique identifier for the message
    uint64_t MessageData;  // Data payload of the message
} Message;

#endif // MESSAGE_H
