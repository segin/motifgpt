#include "buffer_utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
    if (current_assistant_response_len + len + 1 > current_assistant_response_capacity) {
        current_assistant_response_capacity = (current_assistant_response_len + len + 1) * 2;
        char *new_buf = realloc(current_assistant_response_buffer, current_assistant_response_capacity);
        if (!new_buf) {
            perror("realloc assistant_buffer"); free(current_assistant_response_buffer);
            current_assistant_response_buffer = NULL; current_assistant_response_len = 0; current_assistant_response_capacity = 0;
            return;
        }
        current_assistant_response_buffer = new_buf;
    }
    memcpy(current_assistant_response_buffer + current_assistant_response_len, text, len);
    current_assistant_response_len += len;
    current_assistant_response_buffer[current_assistant_response_len] = '\0';
}
