#include "../motifgpt_history.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

// Configuration variables (defined elsewhere, e.g., in motifgpt.c or tests)
int current_max_history_messages = 10;
bool history_limits_disabled = false;

// Mock disasterparty functions since we don't link with the real library for this unit test
bool dp_message_add_text_part(dp_message_t *msg, const char *text) {
    if (!msg) return false;
    dp_content_part_t *new_parts = realloc(msg->parts, (msg->num_parts + 1) * sizeof(dp_content_part_t));
    if (!new_parts) return false;
    msg->parts = new_parts;
    msg->parts[msg->num_parts].type = DP_CONTENT_PART_TEXT;
    msg->parts[msg->num_parts].text = text ? strdup(text) : NULL;
    msg->num_parts++;
    return true;
}

bool dp_message_add_base64_image_part(dp_message_t *msg, const char *mime_type, const char *base64_data) {
    if (!msg) return false;
    dp_content_part_t *new_parts = realloc(msg->parts, (msg->num_parts + 1) * sizeof(dp_content_part_t));
    if (!new_parts) return false;
    msg->parts = new_parts;
    msg->parts[msg->num_parts].type = DP_CONTENT_PART_IMAGE_BASE64;
    msg->parts[msg->num_parts].text = strdup("[IMAGE]"); // For simplicity in mock
    msg->num_parts++;
    return true;
}

void dp_free_messages(dp_message_t *messages, size_t count) {
    if (!messages) return;
    for (size_t i = 0; i < count; i++) {
        if (messages[i].parts) {
            for (size_t j = 0; j < messages[i].num_parts; j++) {
                if (messages[i].parts[j].text) free(messages[i].parts[j].text);
            }
            free(messages[i].parts);
        }
    }
}

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

int main() {
    printf("Starting tests...\n");
    test_add_message_text_only();
    test_history_limit_enforcement();
    printf("All tests passed successfully.\n");
    return 0;
}
