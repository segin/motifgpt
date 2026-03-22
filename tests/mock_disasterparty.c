#include "disasterparty.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
    // For mock, just store something to indicate success
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "[IMAGE: %s, data_len=%zu]", mime_type ? mime_type : "null", base64_data ? strlen(base64_data) : 0);
    msg->parts[msg->num_parts].text = strdup(buffer);
    msg->num_parts++;
    return true;
}

void dp_free_messages(dp_message_t *messages, size_t count) {
    if (!messages) return;
    for (size_t i = 0; i < count; i++) {
        if (messages[i].parts) {
            for (size_t j = 0; j < messages[i].num_parts; j++) {
                if (messages[i].parts[j].text) {
                    free(messages[i].parts[j].text);
                }
            }
            free(messages[i].parts);
        }
    }
    // We do NOT free `messages` itself here because it might be part of an array allocated by the caller (like chat_history).
    // The caller (history.c) manages the array allocation.
    // Wait, motifgpt.c calls `dp_free_messages(&chat_history[i], 1)` in loop.
    // And `dp_free_messages(chat_history, chat_history_count)` in free_chat_history.
    // The library function likely frees the content of messages, but NOT the array itself unless it allocated it?
    // Actually, `dp_free_messages` usually frees the array too if it takes `messages` pointer.
    // But `remove_oldest_history_messages` calls it on `&chat_history[i]`.
    // If `dp_free_messages` frees the pointer passed, then `&chat_history[i]` would be invalid if it tries to free a pointer into the middle of an array.
    // So `dp_free_messages` probably only frees the *contents* (deep free) but not the `messages` pointer itself if it's not the start of allocation?
    // Or maybe it does `free(messages)` at the end.
    // If it does, `remove_oldest_history_messages` is BUGGY because it passes `&chat_history[i]`.
    // Let's check `remove_oldest_history_messages` in `motifgpt.c`.

    /*
    void remove_oldest_history_messages(int count_to_remove) {
        if (count_to_remove <= 0 || count_to_remove > chat_history_count) return;
        for (int i = 0; i < count_to_remove; ++i) dp_free_messages(&chat_history[i], 1);
        int remaining_count = chat_history_count - count_to_remove;
        if (remaining_count > 0) memmove(chat_history, &chat_history[count_to_remove], remaining_count * sizeof(dp_message_t));
        chat_history_count = remaining_count;
    }
    */

    // It calls `dp_free_messages(&chat_history[i], 1)`.
    // If `dp_free_messages` frees the pointer, this is Double Free or Invalid Free.
    // So `dp_free_messages` MUST NOT free the pointer `messages` if count is 1?
    // Or maybe `dp_free_messages` never frees the array pointer, just the contents?
    // But `free_chat_history` calls:
    /*
    void free_chat_history() {
        if (chat_history) {
            dp_free_messages(chat_history, chat_history_count);
            free(chat_history); chat_history = NULL;
        }
        ...
    }
    */
    // Here `free(chat_history)` is called explicitly.
    // So `dp_free_messages` DOES NOT free the array pointer. It only frees contents.
    // So my mock implementation is correct (only freeing parts).
}

int dp_serialize_messages_to_fd(dp_message_t *msgs, size_t count, int fd) {
    if (fd < 0) return -1;
    // For mock, just write some data
    const char *data = "mock serialize data\n";
    if (write(fd, data, strlen(data)) == (ssize_t)strlen(data)) return 0;
    return -1;
}

int dp_serialize_messages_to_file(dp_message_t *msgs, size_t count, const char *filename) {
    if (!filename) return -1;
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) return -1;
    int ret = dp_serialize_messages_to_fd(msgs, count, fd);
    close(fd);
    return ret;
}
