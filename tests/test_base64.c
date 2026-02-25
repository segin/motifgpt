#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../buffer_utils.h"

void test_simple_string() {
    const char *input = "Hello World";
    char *output = base64_encode((const unsigned char*)input, strlen(input));
    printf("Test 'Hello World': %s\n", output);
    assert(strcmp(output, "SGVsbG8gV29ybGQ=") == 0);
    free(output);
}

void test_empty_string() {
    const char *input = "";
    char *output = base64_encode((const unsigned char*)input, strlen(input));
    printf("Test empty string: '%s'\n", output);
    assert(strcmp(output, "") == 0);
    free(output);
}

void test_padding_1() {
    // "Hello" length 5. 5%3 = 2. 1 padding char '='.
    const char *input = "Hello";
    char *output = base64_encode((const unsigned char*)input, strlen(input));
    printf("Test 'Hello': %s\n", output);
    assert(strcmp(output, "SGVsbG8=") == 0);
    free(output);
}

void test_padding_2() {
    // "Hell" length 4. 4%3 = 1. 2 padding chars '=='.
    const char *input = "Hell";
    char *output = base64_encode((const unsigned char*)input, strlen(input));
    printf("Test 'Hell': %s\n", output);
    assert(strcmp(output, "SGVsbA==") == 0);
    free(output);
}

void test_binary_data() {
    unsigned char input[] = {0, 1, 2, 3, 255};
    // 00 01 02 03 FF -> AAECA/8=
    char *output = base64_encode(input, sizeof(input));
    printf("Test binary: %s\n", output);
    assert(strcmp(output, "AAECA/8=") == 0);
    free(output);
}

int main() {
    printf("Running base64 tests...\n");
    test_simple_string();
    test_empty_string();
    test_padding_1();
    test_padding_2();
    test_binary_data();
    printf("All tests passed!\n");
    return 0;
}
