#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdbool.h>

/*
 * Reads a file into a buffer.
 * filename: Path to the file.
 * file_size: Output parameter for the size of the read file.
 * Returns: Pointer to the buffer containing file data (caller must free), or NULL on error.
 */
unsigned char* read_file_to_buffer(const char* filename, size_t* file_size);

/*
 * Base64 encodes data.
 * data: Input data.
 * input_length: Length of input data.
 * Returns: Base64 encoded string (caller must free), or NULL on error.
 */
char* base64_encode(const unsigned char *data, size_t input_length);

/*
 * Detects image MIME type from magic numbers.
 * buffer: File content buffer.
 * len: Length of the buffer.
 * Returns: MIME type string ("image/png", "image/jpeg", "image/gif") or NULL if unknown.
 */
const char* get_image_mime_type(const unsigned char* buffer, size_t len);

#endif
