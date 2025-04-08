#ifndef CUSTOM_CONVECTORS_H
#define CUSTOM_CONVECTORS_H

#include <stdint.h>         // For uint64_t
#include <arpa/inet.h>      // For htonl, ntohl, htons, ntohs

/* Convert a 64-bit integer from host to network byte order */
static inline uint64_t htonll(uint64_t hostlonglong) {
    // Check if the system is little-endian (most common on x86 architectures)
    // If little-endian, we need to swap the bytes; if big-endian, no swap is needed
    uint32_t test = 1;
    if (*(uint8_t*)&test == 1) {  // Little-endian system
        // Swap the bytes of the 64-bit value
        uint32_t high = htonl((uint32_t)(hostlonglong >> 32));  // High 32 bits
        uint32_t low = htonl((uint32_t)(hostlonglong & 0xFFFFFFFF));  // Low 32 bits
        return ((uint64_t)low << 32) | (uint64_t)high;
    }
    return hostlonglong;  // Big-endian system, no swap needed
}

/* Convert a 64-bit integer from network to host byte order */
static inline uint64_t ntohll(uint64_t netlonglong) {
    // Since htonll and ntohll are symmetric, we can reuse the same logic
    return htonll(netlonglong);
}

#endif // CUSTOM_CONVECTORS_H