#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

unsigned char* read_file_to_buffer(const char* filename, size_t* file_size) {
    FILE* f = fopen(filename, "rb");
    if (!f) { perror("fopen read_file"); return NULL; }
    fseek(f, 0, SEEK_END); long size = ftell(f);
    if (size < 0 || size > 20 * 1024 * 1024) {
        fclose(f); fprintf(stderr, "File too large (max 20MB) or ftell error.\n"); return NULL;
    }
    *file_size = (size_t)size; fseek(f, 0, SEEK_SET);
    unsigned char* buffer = malloc(*file_size ? *file_size : 1);
    if (!buffer) { fclose(f); perror("malloc read_file"); return NULL; }
    if (*file_size > 0) {
        if (fread(buffer, 1, *file_size, f) != *file_size) {
            fclose(f); free(buffer); fprintf(stderr, "fread error.\n"); return NULL;
        }
    }
    fclose(f); return buffer;
}

char* base64_encode(const unsigned char *data, size_t input_length) {
    const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t output_length = 4 * ((input_length + 2) / 3);
    char *encoded_data = malloc(output_length + 1);
    if (!encoded_data) { perror("malloc base64"); return NULL; }
    for (size_t i = 0, j = 0; i < input_length;) {
        uint32_t octet_a = i < input_length ? data[i++] : 0;
        uint32_t octet_b = i < input_length ? data[i++] : 0;
        uint32_t octet_c = i < input_length ? data[i++] : 0;
        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;
        encoded_data[j++] = base64_chars[(triple >> 18) & 0x3F];
        encoded_data[j++] = base64_chars[(triple >> 12) & 0x3F];
        encoded_data[j++] = base64_chars[(triple >> 6) & 0x3F];
        encoded_data[j++] = base64_chars[(triple >> 0) & 0x3F];
    }
    for (size_t i = 0; i < (3 - input_length % 3) % 3; i++) encoded_data[output_length - 1 - i] = '=';
    encoded_data[output_length] = '\0';
    return encoded_data;
}
