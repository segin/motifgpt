#ifndef BUFFER_UTILS_H
#define BUFFER_UTILS_H

#include <stddef.h>

unsigned char* read_file_to_buffer(const char* filename, size_t* file_size);
char* base64_encode(const unsigned char *data, size_t input_length);

#endif
