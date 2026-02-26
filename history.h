#ifndef HISTORY_H
#define HISTORY_H

#include <stddef.h>
#include <stdbool.h>
#include "disasterparty.h"

// Define max history capacity if not already defined (usually in motifgpt.c, but moving it here)
#ifndef INTERNAL_MAX_HISTORY_CAPACITY
#define INTERNAL_MAX_HISTORY_CAPACITY 10000
#endif

// Global state for chat history
extern dp_message_t *chat_history;
extern int chat_history_count;
extern int chat_history_capacity;

// Configuration variables (defined elsewhere, e.g., in motifgpt.c or test)
extern int current_max_history_messages;
extern bool history_limits_disabled;

// Function prototypes
void add_message_to_history(dp_message_role_t role, const char* text_content, const char* img_mime_type, const char* img_base64_data);
void remove_oldest_history_messages(int count_to_remove);
void free_chat_history();

#endif
