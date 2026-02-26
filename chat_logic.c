#include "chat_logic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

int pipe_fds[2];
bool assistant_is_replying = false;
bool prefix_already_added_for_current_reply = false;
char *current_assistant_response_buffer = NULL;
size_t current_assistant_response_len = 0;
size_t current_assistant_response_capacity = 0;

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

void write_pipe_message(pipe_message_type_t type, const char* data) {
    pipe_message_t msg; msg.type = type;
    if (data) strncpy(msg.data, data, sizeof(msg.data) - 1); else msg.data[0] = '\0';
    msg.data[sizeof(msg.data) - 1] = '\0';
    ssize_t written = write(pipe_fds[1], &msg, sizeof(pipe_message_t));
    if (written != sizeof(pipe_message_t)) {
        if (written == -1) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("write_pipe_message");
            } else {
                fprintf(stderr, "write_pipe_message: Pipe is full or temporarily unavailable (EAGAIN/EWOULDBLOCK).\n");
            }
        } else {
             fprintf(stderr, "write_pipe_message: Partial write to pipe (%ld bytes instead of %zu).\n", written, sizeof(pipe_message_t));
        }
    }
}

int stream_handler(const char* token, void* user_data, bool is_final, const char* error_during_stream) {
    if (error_during_stream) {
        write_pipe_message(PIPE_MSG_ERROR, error_during_stream);
        assistant_is_replying = false; prefix_already_added_for_current_reply = false;
        if (current_assistant_response_buffer) current_assistant_response_buffer[0] = '\0';
        current_assistant_response_len = 0;
        return 1;
    }
    if (!assistant_is_replying) {
        assistant_is_replying = true;
        prefix_already_added_for_current_reply = false;
        if (current_assistant_response_buffer) current_assistant_response_buffer[0] = '\0';
        current_assistant_response_len = 0;
    }
    if (token) {
        append_to_assistant_buffer(token);
        write_pipe_message(PIPE_MSG_TOKEN, token);
    }
    if (is_final) {
        write_pipe_message(PIPE_MSG_STREAM_END, NULL);
    }
    return 0;
}
