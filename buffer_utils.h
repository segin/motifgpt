#ifndef BUFFER_UTILS_H
#define BUFFER_UTILS_H

#include <stddef.h>

extern char *current_assistant_response_buffer;
extern size_t current_assistant_response_len;
extern size_t current_assistant_response_capacity;

void append_to_assistant_buffer(const char* text);
void init_assistant_buffer();
void free_assistant_buffer();
void reset_assistant_buffer();

#endif
