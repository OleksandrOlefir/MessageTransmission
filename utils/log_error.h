#ifndef LOG_ERROR_H
#define LOG_ERROR_H

#include <errno.h>
#include "custom_output.h"

/* Log an error message with errno details */
void logError(const char* message) {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "[ERROR] %s: %s\n", message, strerror(errno));
    print_err(buffer);
}

#endif // LOG_ERROR_H
