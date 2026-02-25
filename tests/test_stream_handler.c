#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include "../stream_handler.h"
#include "../motifgpt_types.h"

// Mock state
typedef struct {
    pipe_message_type_t last_type;
    char last_data[512];
    int call_count;
} MockPipe;

MockPipe mock_pipe;

void mock_write_cb(pipe_message_type_t type, const char *data) {
    mock_pipe.last_type = type;
    if (data) strncpy(mock_pipe.last_data, data, sizeof(mock_pipe.last_data)-1);
    else mock_pipe.last_data[0] = '\0';
    mock_pipe.call_count++;
}

void reset_mock() {
    mock_pipe.last_type = -1;
    mock_pipe.last_data[0] = '\0';
    mock_pipe.call_count = 0;
}

void test_error_handling() {
    printf("Running test_error_handling...\n");
    bool is_replying = true;
    bool prefix_added = true;
    char *response_buffer = strdup("partial response");
    size_t response_len = strlen(response_buffer);
    size_t response_capacity = 1024;

    StreamContext ctx = {
        .is_replying = &is_replying,
        .prefix_added = &prefix_added,
        .response_buffer = &response_buffer,
        .response_len = &response_len,
        .response_capacity = &response_capacity,
        .write_cb = mock_write_cb
    };

    reset_mock();
    int ret = stream_handler(NULL, &ctx, false, "Network Error");

    assert(ret == 1);
    assert(mock_pipe.last_type == PIPE_MSG_ERROR);
    assert(strcmp(mock_pipe.last_data, "Network Error") == 0);
    assert(is_replying == false);
    assert(prefix_added == false);
    assert(response_len == 0);
    assert(response_buffer[0] == '\0');

    free(response_buffer);
    printf("test_error_handling passed.\n");
}

void test_normal_stream() {
    printf("Running test_normal_stream...\n");
    bool is_replying = false;
    bool prefix_added = false;
    char *response_buffer = malloc(1024);
    if (response_buffer) response_buffer[0] = '\0';
    size_t response_len = 0;
    size_t response_capacity = 1024;

    StreamContext ctx = {
        .is_replying = &is_replying,
        .prefix_added = &prefix_added,
        .response_buffer = &response_buffer,
        .response_len = &response_len,
        .response_capacity = &response_capacity,
        .write_cb = mock_write_cb
    };

    // First token
    reset_mock();
    int ret = stream_handler("Hello", &ctx, false, NULL);
    assert(ret == 0);
    assert(is_replying == true);
    assert(prefix_added == false);

    assert(mock_pipe.last_type == PIPE_MSG_TOKEN);
    assert(strcmp(mock_pipe.last_data, "Hello") == 0);
    assert(strcmp(response_buffer, "Hello") == 0);
    assert(response_len == 5);

    // Second token
    reset_mock();
    stream_handler(" World", &ctx, false, NULL);
    assert(mock_pipe.last_type == PIPE_MSG_TOKEN);
    assert(strcmp(mock_pipe.last_data, " World") == 0);
    assert(strcmp(response_buffer, "Hello World") == 0);
    assert(response_len == 11);

    // End of stream
    reset_mock();
    stream_handler(NULL, &ctx, true, NULL);
    assert(mock_pipe.last_type == PIPE_MSG_STREAM_END);

    free(response_buffer);
    printf("test_normal_stream passed.\n");
}

int main() {
    test_error_handling();
    test_normal_stream();
    return 0;
}
