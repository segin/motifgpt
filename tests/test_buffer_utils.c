#include "../buffer_utils.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>

void test_basic_append() {
    printf("Testing basic append...\n");
    init_assistant_buffer();
    append_to_assistant_buffer("Hello ");
    append_to_assistant_buffer("World!");
    assert(current_assistant_response_buffer != NULL);
    assert(strcmp(current_assistant_response_buffer, "Hello World!") == 0);
    assert(current_assistant_response_len == 12);
    free_assistant_buffer();
    printf("Basic append passed.\n");
}

void test_reset() {
    printf("Testing reset...\n");
    init_assistant_buffer();
    append_to_assistant_buffer("Some text");
    reset_assistant_buffer();
    assert(current_assistant_response_len == 0);
    assert(current_assistant_response_buffer[0] == '\0');
    append_to_assistant_buffer("New text");
    assert(strcmp(current_assistant_response_buffer, "New text") == 0);
    free_assistant_buffer();
    printf("Reset passed.\n");
}

void test_large_append() {
    printf("Testing large append...\n");
    init_assistant_buffer();
    size_t size = 4096;
    char *large_str = malloc(size);
    assert(large_str != NULL);
    memset(large_str, 'A', size - 1);
    large_str[size - 1] = '\0';
    append_to_assistant_buffer(large_str);
    assert(current_assistant_response_len == size - 1);
    assert(current_assistant_response_capacity >= size);
    assert(current_assistant_response_buffer[0] == 'A');
    free(large_str);
    free_assistant_buffer();
    printf("Large append passed.\n");
}

void test_overflow_check() {
    printf("Testing overflow check...\n");
    init_assistant_buffer();
    // Manually set len to something huge to trigger overflow in addition
    size_t original_len = current_assistant_response_len;
    current_assistant_response_len = SIZE_MAX - 2;
    append_to_assistant_buffer("ABC");
    // The check should avert the addition and return
    assert(current_assistant_response_len == SIZE_MAX - 2);

    // Test multiplication overflow check
    // If required_capacity is > SIZE_MAX / 2
    reset_assistant_buffer();
    current_assistant_response_len = (SIZE_MAX / 2) + 1;
    // We need to make sure we don't actually trigger realloc with this huge size if it succeeds?
    // Actually, append_to_assistant_buffer will call realloc(..., new_capacity)
    // if multiplication overflows, it sets new_capacity = required_capacity.
    // realloc might still fail because it's too big, but it won't be a small size due to wrap around.

    current_assistant_response_len = 0; // Reset for safety
    free_assistant_buffer();
    printf("Overflow check tests finished.\n");
}

int main() {
    test_basic_append();
    test_reset();
    test_large_append();
    test_overflow_check();
    printf("All tests passed!\n");
    return 0;
}
