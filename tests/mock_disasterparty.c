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
    // Note: dp_free_messages is designed to perform a deep free of the contents
    // of the messages array (e.g., text parts, image parts). It does NOT
    // deallocate the 'messages' pointer itself. The caller (e.g., history.c)
    // is responsible for managing the allocation and deallocation of the
    // chat_history array.
}
