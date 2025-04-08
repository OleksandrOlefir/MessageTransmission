#ifndef CUSTOM_OUTPUT_H
#define CUSTOM_OUTPUT_H

#include <stdio.h>
#include <string.h>

/* Print a message to stdout */
void print_out(const char* message) {
    printf("%s", message);
}

/* Print an error message to stderr */
void print_err(const char* message) {
    fprintf(stderr, "%s", message);
}

/* Print an integer to stdout with a newline */
void print_out_int(int value) {
    char buffer[32];
    sprintf(buffer, "%d\n", value);
    printf("%s", buffer);
}

#endif // CUSTOM_OUTPUT_H