#ifndef BUFFER_UTILS_H
#define BUFFER_UTILS_H

#include <stddef.h>

typedef struct {
    char *data;
    size_t len;
    size_t capacity;
} assistant_response_buffer_t;

void init_assistant_buffer(assistant_response_buffer_t *buf, size_t initial_capacity);
void free_assistant_buffer(assistant_response_buffer_t *buf);
void append_to_assistant_buffer(assistant_response_buffer_t *buf, const char *text);
void clear_assistant_buffer(assistant_response_buffer_t *buf);

char* base64_encode(const unsigned char *data, size_t input_length);
unsigned char* read_file_to_buffer(const char* filename, size_t* file_size);

#endif // BUFFER_UTILS_H
