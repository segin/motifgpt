#ifndef BUFFER_UTILS_H
#define BUFFER_UTILS_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    char *buffer;
    size_t length;
    size_t capacity;
} assistant_response_buffer_t;

void init_assistant_response_buffer(assistant_response_buffer_t *arb, size_t initial_capacity);
void free_assistant_response_buffer(assistant_response_buffer_t *arb);
void append_to_assistant_buffer(assistant_response_buffer_t *arb, const char* text);

char* base64_encode(const unsigned char *data, size_t input_length);
unsigned char* read_file_to_buffer(const char* filename, size_t* file_size);

#endif
