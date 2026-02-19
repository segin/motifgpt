#include "buffer_utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

char *current_assistant_response_buffer = NULL;
size_t current_assistant_response_len = 0;
size_t current_assistant_response_capacity = 0;

void init_assistant_buffer() {
    current_assistant_response_capacity = 1024;
    current_assistant_response_buffer = malloc(current_assistant_response_capacity);
    if (!current_assistant_response_buffer) {
        perror("malloc assistant_buffer");
        exit(1);
    }
    current_assistant_response_buffer[0] = '\0';
    current_assistant_response_len = 0;
}

void free_assistant_buffer() {
    if (current_assistant_response_buffer) {
        free(current_assistant_response_buffer);
        current_assistant_response_buffer = NULL;
    }
    current_assistant_response_len = 0;
    current_assistant_response_capacity = 0;
}

void reset_assistant_buffer() {
    if (current_assistant_response_buffer) {
        current_assistant_response_buffer[0] = '\0';
    }
    current_assistant_response_len = 0;
}

void append_to_assistant_buffer(const char* text) {
    if (!text) return;
    size_t len = strlen(text);
    size_t required_capacity;

    if (__builtin_add_overflow(current_assistant_response_len, len, &required_capacity) ||
        __builtin_add_overflow(required_capacity, 1, &required_capacity)) {
        fprintf(stderr, "Assistant response buffer overflow averted (addition).\n");
        return;
    }

    if (required_capacity > current_assistant_response_capacity) {
        size_t new_capacity;
        if (__builtin_mul_overflow(required_capacity, 2, &new_capacity)) {
            new_capacity = required_capacity;
        }

        char *new_buf = realloc(current_assistant_response_buffer, new_capacity);
        if (!new_buf) {
            perror("realloc assistant_buffer");
            free(current_assistant_response_buffer);
            current_assistant_response_buffer = NULL;
            current_assistant_response_len = 0;
            current_assistant_response_capacity = 0;
            return;
        }
        current_assistant_response_buffer = new_buf;
        current_assistant_response_capacity = new_capacity;
    }
    memcpy(current_assistant_response_buffer + current_assistant_response_len, text, len);
    current_assistant_response_len += len;
    current_assistant_response_buffer[current_assistant_response_len] = '\0';
}

unsigned char* read_file_to_buffer(const char* filename, size_t* file_size) {
    FILE* f = fopen(filename, "rb");
    if (!f) { perror("fopen read_file"); return NULL; }
    fseek(f, 0, SEEK_END); long size = ftell(f);
    if (size < 0) {
        fclose(f); return NULL;
    }
    if (size > 20 * 1024 * 1024) {
        fclose(f); fprintf(stderr, "File too large (max 20MB)\n"); return NULL;
    }
    *file_size = (size_t)size; fseek(f, 0, SEEK_SET);
    size_t alloc_size = (*file_size == 0) ? 1 : *file_size;
    unsigned char* buffer = malloc(alloc_size);
    if (!buffer) { fclose(f); perror("malloc read_file"); return NULL; }
    if (*file_size > 0) {
        if (fread(buffer, 1, *file_size, f) != *file_size) {
            fclose(f); free(buffer); fprintf(stderr, "fread error.\n"); return NULL;
        }
    }
    fclose(f); return buffer;
}
