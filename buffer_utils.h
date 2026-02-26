#ifndef BUFFER_UTILS_H
#define BUFFER_UTILS_H

#include <stddef.h>

extern char *current_assistant_response_buffer;
extern size_t current_assistant_response_len;
extern size_t current_assistant_response_capacity;

/**
 * Initializes the assistant response buffer.
 */
void init_assistant_buffer();

/**
 * Frees the assistant response buffer.
 */
void free_assistant_buffer();

/**
 * Resets the assistant response buffer without freeing it.
 */
void reset_assistant_buffer();

/**
 * Appends text to the assistant response buffer, with overflow protection.
 * @param text The text to append.
 */
void append_to_assistant_buffer(const char* text);

#endif /* BUFFER_UTILS_H */
