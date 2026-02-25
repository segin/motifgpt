#include "buffer_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

void test_read_file_happy_path() {
    printf("Running test_read_file_happy_path...\n");
    const char *filename = "temp_test_file.txt";
    const char *content = "Hello World";
    FILE *f = fopen(filename, "wb");
    assert(f != NULL);
    fwrite(content, 1, strlen(content), f);
    fclose(f);

    size_t size;
    unsigned char *buffer = read_file_to_buffer(filename, &size);
    assert(buffer != NULL);
    assert(size == strlen(content));
    assert(memcmp(buffer, content, size) == 0);
    free(buffer);
    remove(filename);
    printf("Passed.\n");
}

void test_read_empty_file() {
    printf("Running test_read_empty_file...\n");
    const char *filename = "temp_empty_file.txt";
    FILE *f = fopen(filename, "wb");
    assert(f != NULL);
    fclose(f);

    size_t size;
    unsigned char *buffer = read_file_to_buffer(filename, &size);
    assert(buffer != NULL);
    assert(size == 0);
    free(buffer);
    remove(filename);
    printf("Passed.\n");
}

void test_read_missing_file() {
    printf("Running test_read_missing_file...\n");
    const char *filename = "non_existent_file.txt";
    size_t size;
    unsigned char *buffer = read_file_to_buffer(filename, &size);
    assert(buffer == NULL);
    printf("Passed.\n");
}

void test_base64_encode() {
    printf("Running test_base64_encode...\n");
    const char *input = "Hello World";
    const char *expected = "SGVsbG8gV29ybGQ=";
    char *encoded = base64_encode((const unsigned char*)input, strlen(input));
    assert(encoded != NULL);
    assert(strcmp(encoded, expected) == 0);
    free(encoded);

    // Test padding 1
    input = "A"; // 0x41
    // 01000001 -> 010000 010000 -> Q Q = =
    expected = "QQ==";
    encoded = base64_encode((const unsigned char*)input, strlen(input));
    assert(encoded != NULL);
    assert(strcmp(encoded, expected) == 0);
    free(encoded);

    // Test padding 2
    input = "AB"; // 0x41 0x42
    // 01000001 01000010 -> 010000 010100 001000 -> Q U I =
    expected = "QUI=";
    encoded = base64_encode((const unsigned char*)input, strlen(input));
    assert(encoded != NULL);
    assert(strcmp(encoded, expected) == 0);
    free(encoded);
    printf("Passed.\n");
}

int main() {
    test_read_file_happy_path();
    test_read_empty_file();
    test_read_missing_file();
    test_base64_encode();
    printf("All tests passed!\n");
    return 0;
}
