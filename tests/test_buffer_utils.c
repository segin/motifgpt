#include "../buffer_utils.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void test_initialization() {
    printf("Testing Initialization...\n");
    init_assistant_buffer();
    assert(current_assistant_response_buffer != NULL);
    assert(current_assistant_response_capacity == 1024);
    assert(current_assistant_response_len == 0);
    assert(current_assistant_response_buffer[0] == '\0');
    free_assistant_buffer();
    printf("Initialization Passed.\n");
}

void test_append_simple() {
    printf("Testing Simple Append...\n");
    init_assistant_buffer();
    const char *text = "Hello, World!";
    append_to_assistant_buffer(text);
    assert(current_assistant_response_len == strlen(text));
    assert(strcmp(current_assistant_response_buffer, text) == 0);
    assert(current_assistant_response_capacity == 1024); // Shouldn't resize yet
    free_assistant_buffer();
    printf("Simple Append Passed.\n");
}

void test_append_null() {
    printf("Testing Append NULL...\n");
    init_assistant_buffer();
    append_to_assistant_buffer(NULL);
    assert(current_assistant_response_len == 0);
    assert(current_assistant_response_buffer[0] == '\0');
    free_assistant_buffer();
    printf("Append NULL Passed.\n");
}

void test_reset() {
    printf("Testing Reset...\n");
    init_assistant_buffer();
    append_to_assistant_buffer("Some text");
    assert(current_assistant_response_len > 0);
    reset_assistant_buffer();
    assert(current_assistant_response_len == 0);
    assert(current_assistant_response_buffer[0] == '\0');
    // Verify we can append again after reset
    append_to_assistant_buffer("New text");
    assert(strcmp(current_assistant_response_buffer, "New text") == 0);
    free_assistant_buffer();
    printf("Reset Passed.\n");
}

void test_realloc() {
    printf("Testing Reallocation...\n");
    init_assistant_buffer();
    size_t initial_cap = current_assistant_response_capacity;

    // Create a large string
    size_t large_size = initial_cap + 100;
    char *large_str = malloc(large_size + 1);
    memset(large_str, 'A', large_size);
    large_str[large_size] = '\0';

    append_to_assistant_buffer(large_str);

    assert(current_assistant_response_len == large_size);
    assert(current_assistant_response_capacity > initial_cap);
    assert(strncmp(current_assistant_response_buffer, large_str, large_size) == 0);

    free(large_str);
    free_assistant_buffer();
    printf("Reallocation Passed.\n");
}

void test_multiple_appends() {
    printf("Testing Multiple Appends...\n");
    init_assistant_buffer();
    append_to_assistant_buffer("Part 1");
    append_to_assistant_buffer(" Part 2");
    assert(strcmp(current_assistant_response_buffer, "Part 1 Part 2") == 0);
    assert(current_assistant_response_len == strlen("Part 1 Part 2"));
    free_assistant_buffer();
    printf("Multiple Appends Passed.\n");
}

int main() {
    test_initialization();
    test_append_simple();
    test_append_null();
    test_reset();
    test_realloc();
    test_multiple_appends();
    printf("All tests passed!\n");
    return 0;
}
