#ifndef CHAT_LOGIC_H
#define CHAT_LOGIC_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    PIPE_MSG_TOKEN,
    PIPE_MSG_STREAM_END,
    PIPE_MSG_ERROR,
    PIPE_MSG_MODEL_LIST_ITEM,
    PIPE_MSG_MODEL_LIST_END,
    PIPE_MSG_MODEL_LIST_ERROR
} pipe_message_type_t;

typedef struct {
    pipe_message_type_t type;
    char data[512];
} pipe_message_t;

extern int pipe_fds[2];
extern bool assistant_is_replying;
extern bool prefix_already_added_for_current_reply;
extern char *current_assistant_response_buffer;
extern size_t current_assistant_response_len;
extern size_t current_assistant_response_capacity;

void append_to_assistant_buffer(const char* text);
void write_pipe_message(pipe_message_type_t type, const char* data);
int stream_handler(const char* token, void* user_data, bool is_final, const char* error_during_stream);

#endif
