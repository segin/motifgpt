#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdbool.h>

#define MAX_FILE_SIZE_BYTES (20 * 1024 * 1024)

/**
 * Generates the system prompt for the LLM.
 * @param buffer The buffer to store the generated prompt.
 * @param buffer_size The size of the buffer.
 * @param custom_prompt The user-defined system prompt.
 * @param append_default Whether to append the default prompt to the custom prompt.
 */
void generate_system_prompt(char *buffer, size_t buffer_size, const char *custom_prompt, bool append_default);

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
