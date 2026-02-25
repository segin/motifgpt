#include "history.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Globals
dp_message_t *chat_history = NULL;
int chat_history_count = 0;
int chat_history_capacity = 0;
int current_max_history_messages = DEFAULT_MAX_HISTORY_MESSAGES;
bool history_limits_disabled = false;

void remove_oldest_history_messages(int count_to_remove) {
    if (count_to_remove <= 0 || count_to_remove > chat_history_count) return;
    for (int i = 0; i < count_to_remove; ++i) dp_free_messages(&chat_history[i], 1);
    int remaining_count = chat_history_count - count_to_remove;
    if (remaining_count > 0) memmove(chat_history, &chat_history[count_to_remove], remaining_count * sizeof(dp_message_t));
    chat_history_count = remaining_count;
}

void add_message_to_history(dp_message_role_t role, const char* text_content, const char* img_mime_type, const char* img_base64_data) {
    int effective_max_history = history_limits_disabled ? INTERNAL_MAX_HISTORY_CAPACITY : current_max_history_messages;
    if (chat_history_count >= effective_max_history && effective_max_history > 0 && !history_limits_disabled ) {
        int messages_to_remove = (chat_history_count - effective_max_history) + 1;
        if (chat_history_count < messages_to_remove) messages_to_remove = chat_history_count;
        if (messages_to_remove > 0) {
             printf("Chat history limit (%d) reached. Removing %d oldest message(s).\n", effective_max_history, messages_to_remove);
             remove_oldest_history_messages(messages_to_remove);
        }
    }
    if (chat_history_count >= chat_history_capacity) {
        chat_history_capacity = (chat_history_capacity == 0) ? 10 : chat_history_capacity * 2;
        if (chat_history_capacity > INTERNAL_MAX_HISTORY_CAPACITY) chat_history_capacity = INTERNAL_MAX_HISTORY_CAPACITY;
        if (chat_history_count >= chat_history_capacity) {
            fprintf(stderr, "Cannot expand history further due to internal capacity limit.\n"); return;
        }
        dp_message_t *new_history = realloc(chat_history, chat_history_capacity * sizeof(dp_message_t));
        if (!new_history) { perror("realloc chat_history"); return; }
        chat_history = new_history;
    }
    dp_message_t *new_msg = &chat_history[chat_history_count];
    new_msg->role = role; new_msg->num_parts = 0; new_msg->parts = NULL;
    bool success = true;
    if ((text_content && strlen(text_content) > 0) || (role == DP_ROLE_ASSISTANT && text_content != NULL) ) {
        if (!dp_message_add_text_part(new_msg, text_content)) {
            fprintf(stderr, "Failed to add text part to history.\n"); success = false;
        }
    }
    if (success && img_base64_data && img_mime_type) {
        if (!dp_message_add_base64_image_part(new_msg, img_mime_type, img_base64_data)) {
            fprintf(stderr, "Failed to add image part to history.\n"); success = false;
        }
    }
    if (success && new_msg->num_parts > 0) {
        chat_history_count++;
    } else if (new_msg->parts) {
        free(new_msg->parts); new_msg->parts = NULL;
    } else if (!text_content && !img_base64_data && role == DP_ROLE_USER) { return; }
}

void free_chat_history() {
    if (chat_history) {
        dp_free_messages(chat_history, chat_history_count);
        free(chat_history); chat_history = NULL;
    }
    chat_history_count = 0; chat_history_capacity = 0;
}
