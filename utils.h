#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/**
 * Reads a file into a newly allocated buffer.
 *
 * @param filename The path to the file to read.
 * @param file_size Output parameter to store the size of the file.
 * @return A pointer to the buffer containing the file content, or NULL on error.
 *         The caller is responsible for freeing the buffer.
 */
unsigned char* read_file_to_buffer(const char* filename, size_t* file_size);

/**
 * Encodes binary data to a Base64 string.
 *
 * @param data The input binary data.
 * @param input_length The length of the input data.
 * @return A pointer to the newly allocated null-terminated Base64 string, or NULL on error.
 *         The caller is responsible for freeing the string.
 */
char* base64_encode(const unsigned char *data, size_t input_length);

#endif // UTILS_H
