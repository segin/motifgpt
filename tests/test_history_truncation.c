#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "history.h"
#include "disasterparty.h"

// Mock dp_free_messages
int free_messages_called = 0;
int free_messages_count_arg = 0;

void dp_free_messages(dp_message_t *messages, size_t count) {
    free_messages_called++;
    free_messages_count_arg += (int)count;
}

// Helper to reset state
void reset_test_state() {
    if (chat_history) {
        free(chat_history);
        chat_history = NULL;
    }
    chat_history_count = 0;
    chat_history_capacity = 0;
    free_messages_called = 0;
    free_messages_count_arg = 0;
}

void test_remove_none() {
    reset_test_state();
    chat_history_count = 5;
    remove_oldest_history_messages(0);
    assert(chat_history_count == 5);
    assert(free_messages_called == 0);
    printf("test_remove_none passed\n");
}

void test_remove_invalid() {
    reset_test_state();
    chat_history_count = 5;
    remove_oldest_history_messages(-1);
    assert(chat_history_count == 5);
    remove_oldest_history_messages(6);
    assert(chat_history_count == 5);
    assert(free_messages_called == 0);
    printf("test_remove_invalid passed\n");
}

void test_remove_all() {
    reset_test_state();
    chat_history = calloc(5, sizeof(dp_message_t));
    chat_history_count = 5;

    // We expect it to iterate count_to_remove times and call free on each message individually with count 1
    // remove_oldest_history_messages implementation: for (int i = 0; i < count_to_remove; ++i) dp_free_messages(&chat_history[i], 1);

    remove_oldest_history_messages(5);
    assert(chat_history_count == 0);
    assert(free_messages_called == 5);

    reset_test_state();
    printf("test_remove_all passed\n");
}

void test_remove_partial() {
    reset_test_state();
    chat_history = calloc(5, sizeof(dp_message_t));
    chat_history_count = 5;
    // Mark messages to identify them
    chat_history[0].role = DP_ROLE_USER; // oldest
    chat_history[1].role = DP_ROLE_ASSISTANT;
    chat_history[2].role = DP_ROLE_USER;
    chat_history[3].role = DP_ROLE_ASSISTANT;
    chat_history[4].role = DP_ROLE_USER; // newest

    remove_oldest_history_messages(2);

    assert(chat_history_count == 3);
    assert(free_messages_called == 2);

    // Verify shift
    // The messages at index 2, 3, 4 should now be at 0, 1, 2
    assert(chat_history[0].role == DP_ROLE_USER); // was index 2
    assert(chat_history[1].role == DP_ROLE_ASSISTANT); // was index 3
    assert(chat_history[2].role == DP_ROLE_USER); // was index 4

    reset_test_state();
    printf("test_remove_partial passed\n");
}

int main() {
    // Setup initial state
    chat_history = NULL;
    chat_history_count = 0;

    test_remove_none();
    test_remove_invalid();
    test_remove_all();
    test_remove_partial();

    printf("All tests passed!\n");
    return 0;
}
