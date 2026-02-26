#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Mock dp_message_t
typedef struct {
    int id;
    int freed; // Track if this message has been freed
} dp_message_t;

// Mock globals
dp_message_t *chat_history = NULL;
int chat_history_count = 0;
int chat_history_capacity = 0;

// Mock dp_free_messages
void dp_free_messages(dp_message_t *messages, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        messages[i].freed = 1;
        // In reality this would free internal pointers, here we just mark it
    }
}

// Include function under test
#include "../motifgpt_history.c"

// Helper to setup history
void setup_history(int count) {
    if (chat_history) free(chat_history);
    chat_history_count = count;
    chat_history_capacity = count + 10;
    chat_history = malloc(chat_history_capacity * sizeof(dp_message_t));
    for (int i = 0; i < count; ++i) {
        chat_history[i].id = i;
        chat_history[i].freed = 0;
    }
}

void test_remove_zero() {
    setup_history(5);
    remove_oldest_history_messages(0);
    assert(chat_history_count == 5);
    for(int i=0; i<5; ++i) assert(chat_history[i].freed == 0);
    printf("test_remove_zero passed\n");
}

void test_remove_negative() {
    setup_history(5);
    remove_oldest_history_messages(-1);
    assert(chat_history_count == 5);
    for(int i=0; i<5; ++i) assert(chat_history[i].freed == 0);
    printf("test_remove_negative passed\n");
}

void test_remove_more_than_count() {
    setup_history(5);
    remove_oldest_history_messages(6);
    assert(chat_history_count == 5); // Should do nothing
    for(int i=0; i<5; ++i) assert(chat_history[i].freed == 0);
    printf("test_remove_more_than_count passed\n");
}

void test_remove_all() {
    setup_history(5);
    remove_oldest_history_messages(5);
    assert(chat_history_count == 0);
    // Since count is 0, no memmove happened. The array still holds the old structs which were freed.
    for(int i=0; i<5; ++i) assert(chat_history[i].freed == 1);
    printf("test_remove_all passed\n");
}

void test_remove_partial() {
    setup_history(5);
    // remove 2 oldest (id 0 and 1)
    // remaining should be id 2, 3, 4
    remove_oldest_history_messages(2);
    assert(chat_history_count == 3);
    assert(chat_history[0].id == 2);
    assert(chat_history[1].id == 3);
    assert(chat_history[2].id == 4);

    // Check that we didn't free the messages that are supposed to remain.
    // The ones moved from 2,3,4 had freed=0.
    assert(chat_history[0].freed == 0);
    assert(chat_history[1].freed == 0);
    assert(chat_history[2].freed == 0);

    printf("test_remove_partial passed\n");
}

int main() {
    test_remove_zero();
    test_remove_negative();
    test_remove_more_than_count();
    test_remove_all();
    test_remove_partial();

    if (chat_history) free(chat_history);
    printf("All tests passed.\n");
    return 0;
}
