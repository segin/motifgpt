#include "../buffer_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

void test_initialization() {
    printf("Testing buffer initialization...\n");
    init_assistant_buffer();
    assert(current_assistant_response_buffer != NULL);
    assert(current_assistant_response_len == 0);
    assert(current_assistant_response_capacity >= 1024);
    assert(current_assistant_response_buffer[0] == '\0');
    printf("Initialization Passed.\n");
}

void test_append_simple() {
    printf("Testing simple append...\n");
    append_to_assistant_buffer("Hello");
    assert(strcmp(current_assistant_response_buffer, "Hello") == 0);
    assert(current_assistant_response_len == 5);
    append_to_assistant_buffer(" World");
    assert(strcmp(current_assistant_response_buffer, "Hello World") == 0);
    assert(current_assistant_response_len == 11);
    printf("Simple Append Passed.\n");
}

void test_append_null() {
    printf("Testing NULL append...\n");
    size_t old_len = current_assistant_response_len;
    append_to_assistant_buffer(NULL);
    assert(current_assistant_response_len == old_len);
    printf("NULL Append Passed.\n");
}

void test_reset() {
    printf("Testing buffer reset...\n");
    reset_assistant_buffer();
    assert(current_assistant_response_len == 0);
    assert(current_assistant_response_buffer[0] == '\0');
    printf("Reset Passed.\n");
}

void test_realloc() {
    printf("Testing buffer reallocation...\n");
    reset_assistant_buffer();
    char large_str[2048];
    memset(large_str, 'A', sizeof(large_str) - 1);
    large_str[sizeof(large_str) - 1] = '\0';

    append_to_assistant_buffer(large_str);
    assert(current_assistant_response_len == 2047);
    assert(current_assistant_response_capacity >= 2048);
    printf("Reallocation Passed.\n");
}

void test_multiple_appends() {
    printf("Testing multiple appends...\n");
    reset_assistant_buffer();
    for (int i = 0; i < 100; i++) {
        append_to_assistant_buffer("X");
    }
    assert(current_assistant_response_len == 100);
    assert(strlen(current_assistant_response_buffer) == 100);
    printf("Multiple Appends Passed.\n");
}

int main() {
    test_initialization();
    test_append_simple();
    test_append_null();
    test_reset();
    test_realloc();
    test_multiple_appends();
    free_assistant_buffer();
    printf("All tests passed!\n");
    return 0;
}
