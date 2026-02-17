#include "history.h"
#include "disasterparty.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

// Mock globals needed by history.c
int current_max_history_messages = 10;
bool history_limits_disabled = false;

// Helper to reset state
void reset_history() {
    free_chat_history();
    chat_history_capacity = 0;
    chat_history_count = 0;
    current_max_history_messages = 10;
    history_limits_disabled = false;
}

void test_add_message_text_only() {
    printf("Running test_add_message_text_only...\n");
    reset_history();

    add_message_to_history(DP_ROLE_USER, "Hello World", NULL, NULL);

    assert(chat_history_count == 1);
    assert(chat_history[0].role == DP_ROLE_USER);
    assert(chat_history[0].num_parts == 1);
    assert(chat_history[0].parts[0].type == DP_CONTENT_PART_TEXT);
    assert(strcmp(chat_history[0].parts[0].text, "Hello World") == 0);

    printf("test_add_message_text_only passed.\n");
}

void test_add_message_image_only() {
    printf("Running test_add_message_image_only...\n");
    reset_history();

    const char *mime = "image/png";
    const char *b64 = "base64data";

    add_message_to_history(DP_ROLE_USER, NULL, mime, b64);

    assert(chat_history_count == 1);
    assert(chat_history[0].num_parts == 1);
    assert(chat_history[0].parts[0].type == DP_CONTENT_PART_IMAGE_BASE64);
    // Mock stores description in text field
    assert(strstr(chat_history[0].parts[0].text, "image/png") != NULL);

    printf("test_add_message_image_only passed.\n");
}

void test_add_message_mixed() {
    printf("Running test_add_message_mixed...\n");
    reset_history();

    add_message_to_history(DP_ROLE_USER, "Caption", "image/png", "data");

    assert(chat_history_count == 1);
    assert(chat_history[0].num_parts == 2);
    assert(chat_history[0].parts[0].type == DP_CONTENT_PART_TEXT);
    assert(strcmp(chat_history[0].parts[0].text, "Caption") == 0);
    assert(chat_history[0].parts[1].type == DP_CONTENT_PART_IMAGE_BASE64);

    printf("test_add_message_mixed passed.\n");
}

void test_history_limit_enforcement() {
    printf("Running test_history_limit_enforcement...\n");
    reset_history();
    current_max_history_messages = 2;

    add_message_to_history(DP_ROLE_USER, "Msg 1", NULL, NULL);
    add_message_to_history(DP_ROLE_ASSISTANT, "Msg 2", NULL, NULL);
    assert(chat_history_count == 2);

    // Add 3rd message, should remove oldest (Msg 1)
    add_message_to_history(DP_ROLE_USER, "Msg 3", NULL, NULL);

    assert(chat_history_count == 2);
    assert(strcmp(chat_history[0].parts[0].text, "Msg 2") == 0); // Oldest became Msg 2
    assert(strcmp(chat_history[1].parts[0].text, "Msg 3") == 0);

    printf("test_history_limit_enforcement passed.\n");
}

void test_history_limits_disabled() {
    printf("Running test_history_limits_disabled...\n");
    reset_history();
    current_max_history_messages = 2;
    history_limits_disabled = true;

    add_message_to_history(DP_ROLE_USER, "Msg 1", NULL, NULL);
    add_message_to_history(DP_ROLE_ASSISTANT, "Msg 2", NULL, NULL);
    add_message_to_history(DP_ROLE_USER, "Msg 3", NULL, NULL);

    assert(chat_history_count == 3); // No removal
    assert(strcmp(chat_history[0].parts[0].text, "Msg 1") == 0);

    printf("test_history_limits_disabled passed.\n");
}

void test_remove_oldest_logic() {
    printf("Running test_remove_oldest_logic...\n");
    reset_history();

    add_message_to_history(DP_ROLE_USER, "1", NULL, NULL);
    add_message_to_history(DP_ROLE_USER, "2", NULL, NULL);
    add_message_to_history(DP_ROLE_USER, "3", NULL, NULL);

    remove_oldest_history_messages(2);

    assert(chat_history_count == 1);
    assert(strcmp(chat_history[0].parts[0].text, "3") == 0);

    printf("test_remove_oldest_logic passed.\n");
}

void test_internal_capacity_limit() {
    printf("Running test_internal_capacity_limit...\n");
    reset_history();
    history_limits_disabled = true;
    // Mock INTERNAL_MAX_HISTORY_CAPACITY is 10000.
    // We cannot easily test 10000 messages quickly in unit test without spamming.
    // But we can check if capacity expansion logic works.

    // Force capacity to be small (normally it starts at 10 or doubles)
    // We can't control realloc logic parameters directly as they are hardcoded (10, *2).
    // But we can simulate.

    for (int i = 0; i < 15; i++) {
        char buf[16];
        snprintf(buf, sizeof(buf), "Msg %d", i);
        add_message_to_history(DP_ROLE_USER, buf, NULL, NULL);
    }

    assert(chat_history_count == 15);
    assert(chat_history_capacity >= 15);
    // Capacity logic: 0 -> 10 -> 20.
    assert(chat_history_capacity == 20);

    printf("test_internal_capacity_limit passed.\n");
}

int main() {
    printf("Starting tests...\n");
    test_add_message_text_only();
    test_add_message_image_only();
    test_add_message_mixed();
    test_history_limit_enforcement();
    test_history_limits_disabled();
    test_remove_oldest_logic();
    test_internal_capacity_limit();

    printf("All tests passed successfully.\n");
    return 0;
}
