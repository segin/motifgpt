#ifndef BUFFER_UTILS_H
#define BUFFER_UTILS_H

#include <stddef.h>

/**
 * Base64 encodes the given data.
 * @param data The input data to encode.
 * @param input_length The length of the input data.
 * @return A null-terminated string containing the Base64 encoded data.
 *         The caller is responsible for freeing the returned buffer.
 */
char* base64_encode(const unsigned char *data, size_t input_length);

/**
 * Reads the entire contents of a file into a newly allocated buffer.
 * @param filename The path to the file to read.
 * @param file_size A pointer to a size_t where the file size will be stored.
 * @return A pointer to the buffer containing the file data, or NULL on error.
 *         The caller is responsible for freeing the returned buffer.
 */
unsigned char* read_file_to_buffer(const char* filename, size_t* file_size);

#endif /* BUFFER_UTILS_H */
