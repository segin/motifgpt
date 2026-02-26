#ifndef DISASTERPARTY_H
#define DISASTERPARTY_H

#include <stddef.h>
#include <stdbool.h>

typedef enum {
    DP_ROLE_USER,
    DP_ROLE_ASSISTANT,
    DP_ROLE_SYSTEM
} dp_message_role_t;

typedef enum {
    DP_CONTENT_PART_TEXT,
    DP_CONTENT_PART_IMAGE_BASE64
} dp_content_part_type_t;

typedef struct {
    dp_content_part_type_t type;
    char *text;
    // For mock, we can store base64 data here if needed, or ignore.
    // add_message_to_history calls dp_message_add_base64_image_part(msg, type, data).
    // We can store a description string in `text` for verification.
} dp_content_part_t;

typedef struct {
    dp_message_role_t role;
    dp_content_part_t *parts;
    size_t num_parts;
} dp_message_t;

// Mock functions
bool dp_message_add_text_part(dp_message_t *msg, const char *text);
bool dp_message_add_base64_image_part(dp_message_t *msg, const char *mime_type, const char *base64_data);
void dp_free_messages(dp_message_t *messages, size_t count);

#endif
