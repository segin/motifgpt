#ifndef HISTORY_H
#define HISTORY_H

#include <stdbool.h>
#include <disasterparty.h>

// Constants
#define DEFAULT_MAX_HISTORY_MESSAGES 100
#define INTERNAL_MAX_HISTORY_CAPACITY 10000

// Globals
extern dp_message_t *chat_history;
extern int chat_history_count;
extern int chat_history_capacity;
extern int current_max_history_messages;
extern bool history_limits_disabled;

// Functions
void add_message_to_history(dp_message_role_t role, const char* text_content, const char* img_mime_type, const char* img_base64_data);
void remove_oldest_history_messages(int count_to_remove);
void free_chat_history();

#endif // HISTORY_H
