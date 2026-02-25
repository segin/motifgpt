#ifndef STREAM_HANDLER_H
#define STREAM_HANDLER_H

#include <stdbool.h>
#include <stdlib.h>
#include "motifgpt_types.h"

// Context structure to hold state required by the stream handler
typedef struct {
    bool *is_replying;
    bool *prefix_added;
    char **response_buffer;
    size_t *response_len;
    size_t *response_capacity;
    // Callback to write pipe messages.
    void (*write_cb)(pipe_message_type_t type, const char *data);
} StreamContext;

// The stream handler function matching dp_stream_callback_t signature
int stream_handler(const char* token, void* user_data, bool is_final, const char* error_during_stream);

// Helper function exposed for testing
void append_to_assistant_buffer_in_context(StreamContext *ctx, const char* text);

#endif // STREAM_HANDLER_H
