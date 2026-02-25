#include "../history.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void test_initial_state() {
    assert(chat_history_count == 0);
    assert(chat_history == NULL);
}

void test_add_message() {
    add_message_to_history(DP_ROLE_USER, "Hello", NULL, NULL);
    assert(chat_history_count == 1);
    assert(chat_history[0].role == DP_ROLE_USER);
    // Since we mocked dp_message_add_text_part, we know parts[0] is set
    assert(strcmp(chat_history[0].parts[0].text, "Hello") == 0);
}

void test_capacity_expansion() {
    // Current capacity logic: starts 0, then 10, then *2
    // We already added 1.
    for (int i = 0; i < 20; i++) {
        add_message_to_history(DP_ROLE_ASSISTANT, "Response", NULL, NULL);
    }
    assert(chat_history_capacity >= 21);
    // 10 -> 20 -> 40.
    // At 1 msg: cap 10.
    // At 10 msgs: cap 20.
    // At 20 msgs: cap 40.
    assert(chat_history_capacity >= 21);
}

void test_history_limit() {
    free_chat_history();
    current_max_history_messages = 5;
    history_limits_disabled = false;

    for (int i = 0; i < 5; i++) {
        char buf[20]; sprintf(buf, "Msg %d", i);
        add_message_to_history(DP_ROLE_USER, buf, NULL, NULL);
    }
    assert(chat_history_count == 5);
    assert(strcmp(chat_history[0].parts[0].text, "Msg 0") == 0);

    // Add 6th message, should remove oldest (Msg 0)
    add_message_to_history(DP_ROLE_USER, "Msg 5", NULL, NULL);
    assert(chat_history_count == 5);
    assert(strcmp(chat_history[0].parts[0].text, "Msg 1") == 0);
    assert(strcmp(chat_history[4].parts[0].text, "Msg 5") == 0);
}

void test_clear_history() {
    free_chat_history();
    assert(chat_history_count == 0);
    assert(chat_history == NULL);
}

int main() {
    printf("Running test_initial_state...\n");
    test_initial_state();
    printf("Running test_add_message...\n");
    test_add_message();
    printf("Running test_capacity_expansion...\n");
    test_capacity_expansion();
    printf("Running test_history_limit...\n");
    test_history_limit();
    printf("Running test_clear_history...\n");
    test_clear_history();
    printf("All tests passed!\n");
    return 0;
}
