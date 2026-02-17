#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>

char* base64_encode(const unsigned char *data, size_t input_length);
unsigned char* read_file_to_buffer(const char* filename, size_t* file_size);

#endif
