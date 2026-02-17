#ifndef MOTIFGPT_UTILS_H
#define MOTIFGPT_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

/**
 * Generates the system prompt for the LLM.
 *
 * @param buffer The buffer to store the generated prompt.
 * @param buffer_size The size of the buffer.
 * @param custom_prompt The user-defined system prompt.
 * @param append_default Whether to append the default prompt to the custom prompt.
 */
void generate_system_prompt(char *buffer, size_t buffer_size, const char *custom_prompt, bool append_default);

/**
 * Reads a file into a newly allocated buffer.
 *
 * @param filename The path to the file.
 * @param file_size A pointer to store the size of the file.
 * @return A pointer to the buffer containing the file content, or NULL on error.
 */
unsigned char* read_file_to_buffer(const char* filename, size_t* file_size);

/**
 * Encodes data to Base64.
 *
 * @param data The data to encode.
 * @param input_length The length of the data.
 * @return A newly allocated string containing the Base64 encoded data, or NULL on error.
 */
char* base64_encode(const unsigned char *data, size_t input_length);

#endif // MOTIFGPT_UTILS_H
