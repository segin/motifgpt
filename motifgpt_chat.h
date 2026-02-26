#ifndef MOTIFGPT_CHAT_H
#define MOTIFGPT_CHAT_H

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

/**
 * Appends text to the assistant's response buffer, resizing it if necessary.
 * @param text The text to append.
 */
void append_to_assistant_buffer(const char* text);

/**
 * Writes a message to the internal communication pipe.
 * @param type The type of message.
 * @param data The message data string.
 */
void write_pipe_message(pipe_message_type_t type, const char* data);

/**
 * The callback function used by libdisasterparty to handle streaming tokens.
 * @param token The received token string.
 * @param user_data Optional user-provided data.
 * @param is_final Whether this is the final token in the stream.
 * @param error_during_stream Optional error message if a failure occurred.
 * @return 0 on success, non-zero to abort the stream.
 */
int stream_handler(const char* token, void* user_data, bool is_final, const char* error_during_stream);

#endif /* MOTIFGPT_CHAT_H */
