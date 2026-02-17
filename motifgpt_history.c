void remove_oldest_history_messages(int count_to_remove) {
    if (count_to_remove <= 0 || count_to_remove > chat_history_count) return;
    for (int i = 0; i < count_to_remove; ++i) dp_free_messages(&chat_history[i], 1);
    int remaining_count = chat_history_count - count_to_remove;
    if (remaining_count > 0) memmove(chat_history, &chat_history[count_to_remove], remaining_count * sizeof(dp_message_t));
    chat_history_count = remaining_count;
}
