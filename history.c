#include "history.h"
#include <stdlib.h>
#include <string.h>

dp_message_t *chat_history = NULL;
int chat_history_count = 0;
int chat_history_capacity = 0;

void remove_oldest_history_messages(int count_to_remove) {
    if (count_to_remove <= 0 || count_to_remove > chat_history_count) return;
    for (int i = 0; i < count_to_remove; ++i) dp_free_messages(&chat_history[i], 1);
    int remaining_count = chat_history_count - count_to_remove;
    if (remaining_count > 0) memmove(chat_history, &chat_history[count_to_remove], remaining_count * sizeof(dp_message_t));
    chat_history_count = remaining_count;
}

void free_chat_history() {
    if (chat_history) {
        dp_free_messages(chat_history, chat_history_count);
        free(chat_history); chat_history = NULL;
    }
    chat_history_count = 0; chat_history_capacity = 0;
}
