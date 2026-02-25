#ifndef BUFFER_UTILS_H
#define BUFFER_UTILS_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/*
 * Encodes data to Base64.
 * Returns a null-terminated string which must be freed by the caller.
 * Returns NULL on allocation failure.
 */
char* base64_encode(const unsigned char *data, size_t input_length);

/*
 * Reads a file into a buffer.
 * Returns a buffer which must be freed by the caller.
 * Sets file_size to the number of bytes read.
 * Returns NULL on error (file not found, too large, etc.).
 */
unsigned char* read_file_to_buffer(const char* filename, size_t* file_size);

#endif
