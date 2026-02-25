#include "buffer_utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

void test_init() {
    assistant_response_buffer_t buf;
    init_assistant_buffer(&buf, 100);
    assert(buf.data != NULL);
    assert(buf.capacity >= 100);
    assert(buf.len == 0);
    assert(buf.data[0] == '\0');
    free_assistant_buffer(&buf);
}

void test_append() {
    assistant_response_buffer_t buf;
    init_assistant_buffer(&buf, 10);

    append_to_assistant_buffer(&buf, "Hello");
    assert(strcmp(buf.data, "Hello") == 0);
    assert(buf.len == 5);

    append_to_assistant_buffer(&buf, " World");
    assert(strcmp(buf.data, "Hello World") == 0);
    assert(buf.len == 11);

    // Test resize
    // Capacity starts at 10. "Hello World" is 11 chars + null = 12.
    // So it should have resized.
    assert(buf.capacity >= 12);

    free_assistant_buffer(&buf);
}

void test_clear() {
    assistant_response_buffer_t buf;
    init_assistant_buffer(&buf, 100);
    append_to_assistant_buffer(&buf, "Test");
    assert(buf.len == 4);

    clear_assistant_buffer(&buf);
    assert(buf.len == 0);
    assert(buf.data[0] == '\0');

    free_assistant_buffer(&buf);
}

int main() {
    test_init();
    test_append();
    test_clear();
    printf("test_buffer_utils passed!\n");
    return 0;
}
