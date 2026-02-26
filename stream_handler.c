#include "stream_handler.h"
#include <stdio.h>
#include <string.h>

void append_to_assistant_buffer_in_context(StreamContext *ctx, const char* text) {
    if (!text || !ctx) return;
    size_t len = strlen(text);

    // Check if we need to resize
    if (*(ctx->response_len) + len + 1 > *(ctx->response_capacity)) {
        size_t new_capacity = (*(ctx->response_len) + len + 1) * 2;
        // Handle case where capacity is 0 initially (though motifgpt.c initializes to 1024)
        if (new_capacity == 0) new_capacity = 1024;

        char *new_buf = realloc(*(ctx->response_buffer), new_capacity);
        if (!new_buf) {
            perror("realloc assistant_buffer");
            free(*(ctx->response_buffer));
            *(ctx->response_buffer) = NULL;
            *(ctx->response_len) = 0;
            *(ctx->response_capacity) = 0;
            return;
        }
        *(ctx->response_buffer) = new_buf;
        *(ctx->response_capacity) = new_capacity;
    }

    // Copy new text
    if (*(ctx->response_buffer)) {
        memcpy(*(ctx->response_buffer) + *(ctx->response_len), text, len);
        *(ctx->response_len) += len;
        (*(ctx->response_buffer))[*(ctx->response_len)] = '\0';
    }
}

int stream_handler(const char* token, void* user_data, bool is_final, const char* error_during_stream) {
    StreamContext *ctx = (StreamContext *)user_data;
    if (!ctx) return 1; // Should not happen in normal usage

    if (error_during_stream) {
        if (ctx->write_cb) ctx->write_cb(PIPE_MSG_ERROR, error_during_stream);
        *(ctx->is_replying) = false;
        *(ctx->prefix_added) = false;
        if (*(ctx->response_buffer)) (*(ctx->response_buffer))[0] = '\0';
        *(ctx->response_len) = 0;
        return 1;
    }

    if (!*(ctx->is_replying)) {
        *(ctx->is_replying) = true;
        *(ctx->prefix_added) = false;
        if (*(ctx->response_buffer)) (*(ctx->response_buffer))[0] = '\0';
        *(ctx->response_len) = 0;
    }

    if (token) {
        append_to_assistant_buffer_in_context(ctx, token);
        if (ctx->write_cb) ctx->write_cb(PIPE_MSG_TOKEN, token);
    }

    if (is_final) {
        if (ctx->write_cb) ctx->write_cb(PIPE_MSG_STREAM_END, NULL);
    }
    return 0;
}
