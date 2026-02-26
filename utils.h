#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>

#define MAX_FILE_SIZE_BYTES (20 * 1024 * 1024)

/**
 * Reads a file into a newly allocated buffer.
 * @param filename Path to the file.
 * @param file_size Pointer to store the size of the read file.
 * @return Pointer to the allocated buffer, or NULL on failure.
 */
unsigned char* read_file_to_buffer(const char* filename, size_t* file_size);

/**
 * Encodes data into Base64 format.
 * @param data Data to encode.
 * @param input_length Length of the input data.
 * @return Pointer to the null-terminated Base64 string, or NULL on failure.
 */
char* base64_encode(const unsigned char *data, size_t input_length);

#endif /* UTILS_H */
