#include "../motifgpt_chat.h"
#include "../buffer_utils.h"
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void setup_test_pipe() {
    if (pipe(pipe_fds) == -1) {
        perror("pipe");
        exit(1);
    }
    int flags = fcntl(pipe_fds[0], F_GETFL, 0);
    fcntl(pipe_fds[0], F_SETFL, flags | O_NONBLOCK);
}

void cleanup_test_pipe() {
    close(pipe_fds[0]);
    close(pipe_fds[1]);
}

void test_stream_handler_error() {
    printf("Running test_stream_handler_error...\n");
    setup_test_pipe();
    init_assistant_buffer();

    assistant_is_replying = true;
    prefix_already_added_for_current_reply = true;
    append_to_assistant_buffer("something");

    const char *error_msg = "Stream Error Test";
    int result = stream_handler(NULL, NULL, false, error_msg);

    assert(result == 1);
    assert(assistant_is_replying == false);
    assert(prefix_already_added_for_current_reply == false);
    assert(current_assistant_response_len == 0);
    assert(current_assistant_response_buffer[0] == '\0');

    pipe_message_t msg;
    ssize_t n = read(pipe_fds[0], &msg, sizeof(msg));
    assert(n == sizeof(msg));
    assert(msg.type == PIPE_MSG_ERROR);
    assert(strcmp(msg.data, error_msg) == 0);

    free_assistant_buffer();
    cleanup_test_pipe();
    printf("test_stream_handler_error passed!\n");
}

void test_stream_handler_success_first_token() {
    printf("Running test_stream_handler_success_first_token...\n");
    setup_test_pipe();
    init_assistant_buffer();

    assistant_is_replying = false;
    prefix_already_added_for_current_reply = true; // Should be reset

    const char *token = "Hello";
    int result = stream_handler(token, NULL, false, NULL);

    assert(result == 0);
    assert(assistant_is_replying == true);
    assert(prefix_already_added_for_current_reply == false);
    assert(strcmp(current_assistant_response_buffer, token) == 0);

    pipe_message_t msg;
    ssize_t n = read(pipe_fds[0], &msg, sizeof(msg));
    assert(n == sizeof(msg));
    assert(msg.type == PIPE_MSG_TOKEN);
    assert(strcmp(msg.data, token) == 0);

    free_assistant_buffer();
    cleanup_test_pipe();
    printf("test_stream_handler_success_first_token passed!\n");
}

void test_stream_handler_subsequent_token() {
    printf("Running test_stream_handler_subsequent_token...\n");
    setup_test_pipe();
    init_assistant_buffer();

    assistant_is_replying = true;
    append_to_assistant_buffer("Hello");

    const char *token = " World";
    int result = stream_handler(token, NULL, false, NULL);

    assert(result == 0);
    assert(assistant_is_replying == true);
    assert(strcmp(current_assistant_response_buffer, "Hello World") == 0);

    pipe_message_t msg;
    ssize_t n = read(pipe_fds[0], &msg, sizeof(msg));
    assert(n == sizeof(msg));
    assert(msg.type == PIPE_MSG_TOKEN);
    assert(strcmp(msg.data, token) == 0);

    free_assistant_buffer();
    cleanup_test_pipe();
    printf("test_stream_handler_subsequent_token passed!\n");
}

void test_stream_handler_final_token() {
    printf("Running test_stream_handler_final_token...\n");
    setup_test_pipe();
    init_assistant_buffer();

    assistant_is_replying = true;
    append_to_assistant_buffer("Done");

    int result = stream_handler(NULL, NULL, true, NULL);

    assert(result == 0);
    assert(assistant_is_replying == true);

    pipe_message_t msg;
    ssize_t n = read(pipe_fds[0], &msg, sizeof(msg));
    assert(n == sizeof(msg));
    assert(msg.type == PIPE_MSG_STREAM_END);

    free_assistant_buffer();
    cleanup_test_pipe();
    printf("test_stream_handler_final_token passed!\n");
}

int main() {
    test_stream_handler_error();
    test_stream_handler_success_first_token();
    test_stream_handler_subsequent_token();
    test_stream_handler_final_token();
    return 0;
}
