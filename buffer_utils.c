#include "buffer_utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

char *current_assistant_response_buffer = NULL;
size_t current_assistant_response_len = 0;
size_t current_assistant_response_capacity = 0;
pthread_mutex_t assistant_buffer_mutex = PTHREAD_MUTEX_INITIALIZER;

void init_assistant_buffer() {
    pthread_mutex_lock(&assistant_buffer_mutex);
    current_assistant_response_capacity = 1024;
    current_assistant_response_buffer = malloc(current_assistant_response_capacity);
    if (!current_assistant_response_buffer) {
        perror("malloc assistant_buffer");
        pthread_mutex_unlock(&assistant_buffer_mutex);
        exit(1);
    }
    current_assistant_response_buffer[0] = '\0';
    current_assistant_response_len = 0;
    pthread_mutex_unlock(&assistant_buffer_mutex);
}

void free_assistant_buffer() {
    pthread_mutex_lock(&assistant_buffer_mutex);
    if (current_assistant_response_buffer) {
        free(current_assistant_response_buffer);
        current_assistant_response_buffer = NULL;
    }
    current_assistant_response_len = 0;
    current_assistant_response_capacity = 0;
    pthread_mutex_unlock(&assistant_buffer_mutex);
}

void reset_assistant_buffer() {
    pthread_mutex_lock(&assistant_buffer_mutex);
    if (current_assistant_response_buffer) {
        current_assistant_response_buffer[0] = '\0';
    }
    current_assistant_response_len = 0;
    pthread_mutex_unlock(&assistant_buffer_mutex);
}

void append_to_assistant_buffer(const char* text) {
    if (!text) return;
    pthread_mutex_lock(&assistant_buffer_mutex);
    size_t len = strlen(text);
    size_t required_capacity;

    if (__builtin_add_overflow(current_assistant_response_len, len, &required_capacity) ||
        __builtin_add_overflow(required_capacity, 1, &required_capacity)) {
        fprintf(stderr, "Assistant response buffer overflow averted (addition).\n");
        pthread_mutex_unlock(&assistant_buffer_mutex);
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
            pthread_mutex_unlock(&assistant_buffer_mutex);
            return;
        }
        current_assistant_response_buffer = new_buf;
        current_assistant_response_capacity = new_capacity;
    }
    memcpy(current_assistant_response_buffer + current_assistant_response_len, text, len);
    current_assistant_response_len += len;
    current_assistant_response_buffer[current_assistant_response_len] = '\0';
    pthread_mutex_unlock(&assistant_buffer_mutex);
}
