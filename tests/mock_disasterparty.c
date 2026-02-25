#include <disasterparty.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

void dp_free_messages(dp_message_t *messages, size_t count) {
    if (!messages) return;
    for (size_t i = 0; i < count; i++) {
        if (messages[i].parts) {
            for (size_t j = 0; j < messages[i].num_parts; j++) {
                if (messages[i].parts[j].text) free(messages[i].parts[j].text);
                if (messages[i].parts[j].data) free(messages[i].parts[j].data);
                if (messages[i].parts[j].media_type) free(messages[i].parts[j].media_type);
            }
            free(messages[i].parts);
            messages[i].parts = NULL;
            messages[i].num_parts = 0;
        }
    }
}

bool dp_message_add_text_part(dp_message_t *msg, const char *text) {
    if (!msg || !text) return false;
    dp_content_part_t *new_parts = realloc(msg->parts, (msg->num_parts + 1) * sizeof(dp_content_part_t));
    if (!new_parts) return false;
    msg->parts = new_parts;
    msg->parts[msg->num_parts].type = DP_CONTENT_PART_TEXT;
    msg->parts[msg->num_parts].text = strdup(text);
    msg->parts[msg->num_parts].data = NULL;
    msg->parts[msg->num_parts].media_type = NULL;
    msg->num_parts++;
    return true;
}

bool dp_message_add_base64_image_part(dp_message_t *msg, const char *mime_type, const char *base64_data) {
    if (!msg || !mime_type || !base64_data) return false;
    dp_content_part_t *new_parts = realloc(msg->parts, (msg->num_parts + 1) * sizeof(dp_content_part_t));
    if (!new_parts) return false;
    msg->parts = new_parts;
    msg->parts[msg->num_parts].type = DP_CONTENT_PART_IMAGE_BASE64;
    msg->parts[msg->num_parts].text = NULL;
    msg->parts[msg->num_parts].data = strdup(base64_data);
    msg->parts[msg->num_parts].media_type = strdup(mime_type);
    msg->num_parts++;
    return true;
}
