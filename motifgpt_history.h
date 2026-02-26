#ifndef MOTIFGPT_HISTORY_H
#define MOTIFGPT_HISTORY_H

#include <stddef.h>
#include <stdbool.h>
#include "disasterparty.h"

// Define max history capacity if not already defined
#ifndef INTERNAL_MAX_HISTORY_CAPACITY
#define INTERNAL_MAX_HISTORY_CAPACITY 10000
#endif

// Global state for chat history
extern dp_message_t *chat_history;
extern int chat_history_count;
extern int chat_history_capacity;

// Configuration variables (defined elsewhere, e.g., in motifgpt.c or tests)
extern int current_max_history_messages;
extern bool history_limits_disabled;

// Function prototypes
/**
 * Adds a message to the chat history.
 * @param role The role of the message sender.
 * @param text_content The text content of the message.
 * @param img_mime_type The MIME type of the attached image, or NULL if none.
 * @param img_base64_data The Base64 data of the attached image, or NULL if none.
 */
void add_message_to_history(dp_message_role_t role, const char* text_content, const char* img_mime_type, const char* img_base64_data);

/**
 * Removes the specified number of oldest messages from the chat history.
 * @param count_to_remove The number of messages to remove.
 */
void remove_oldest_history_messages(int count_to_remove);

/**
 * Frees all memory associated with the chat history.
 */
void free_chat_history();

#endif /* MOTIFGPT_HISTORY_H */
