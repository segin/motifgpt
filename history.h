#ifndef HISTORY_H
#define HISTORY_H

#include <stddef.h>
#include "disasterparty.h"

extern dp_message_t *chat_history;
extern int chat_history_count;
extern int chat_history_capacity;

void remove_oldest_history_messages(int count_to_remove);
void free_chat_history();

#endif
