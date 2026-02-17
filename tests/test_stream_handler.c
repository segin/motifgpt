#include "chat_logic.h"
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void test_stream_handler_error() {
    // Setup
    if (pipe(pipe_fds) == -1) {
        perror("pipe");
        exit(1);
    }
    // Set to non-blocking to avoid hanging if read fails
    int flags = fcntl(pipe_fds[0], F_GETFL, 0);
    fcntl(pipe_fds[0], F_SETFL, flags | O_NONBLOCK);

    flags = fcntl(pipe_fds[1], F_GETFL, 0);
    fcntl(pipe_fds[1], F_SETFL, flags | O_NONBLOCK);

    assistant_is_replying = true;
    prefix_already_added_for_current_reply = true;
    current_assistant_response_len = 10;

    // Allocate buffer to simulate ongoing response
    current_assistant_response_capacity = 100;
    current_assistant_response_buffer = malloc(current_assistant_response_capacity);
    strcpy(current_assistant_response_buffer, "something");

    // Execute
    const char *error_msg = "Stream Error Test";
    int result = stream_handler(NULL, NULL, false, error_msg);

    // Verify return value
    assert(result == 1);

    // Verify state changes
    assert(assistant_is_replying == false);
    assert(prefix_already_added_for_current_reply == false);
    assert(current_assistant_response_len == 0);
    assert(current_assistant_response_buffer[0] == '\0');

    // Verify pipe message
    pipe_message_t msg;
    ssize_t n = read(pipe_fds[0], &msg, sizeof(msg));

    if (n != sizeof(msg)) {
        fprintf(stderr, "Read %ld bytes, expected %ld\n", n, sizeof(msg));
        if (n == -1) perror("read");
    }

    assert(n == sizeof(msg));
    assert(msg.type == PIPE_MSG_ERROR);
    assert(strcmp(msg.data, error_msg) == 0);

    // Cleanup
    free(current_assistant_response_buffer);
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    printf("test_stream_handler_error passed!\n");
}

int main() {
    test_stream_handler_error();
    return 0;
}
