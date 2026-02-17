#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../utils.h"

void test_read_existing_file() {
    printf("Running test_read_existing_file...\n");
    const char* filename = "test_file.txt";
    const char* content = "Hello, World!";
    FILE* f = fopen(filename, "wb");
    assert(f != NULL);
    fwrite(content, 1, strlen(content), f);
    fclose(f);

    size_t size = 0;
    unsigned char* buffer = read_file_to_buffer(filename, &size);
    assert(buffer != NULL);
    assert(size == strlen(content));
    assert(memcmp(buffer, content, size) == 0);
    free(buffer);
    remove(filename);
    printf("test_read_existing_file passed.\n");
}

void test_read_empty_file() {
    printf("Running test_read_empty_file...\n");
    const char* filename = "empty_file.txt";
    FILE* f = fopen(filename, "wb");
    assert(f != NULL);
    fclose(f);

    size_t size = 0;
    unsigned char* buffer = read_file_to_buffer(filename, &size);
    assert(buffer != NULL); // Should return allocated 1 byte
    assert(size == 0);
    free(buffer);
    remove(filename);
    printf("test_read_empty_file passed.\n");
}

void test_read_nonexistent_file() {
    printf("Running test_read_nonexistent_file...\n");
    size_t size = 0;
    unsigned char* buffer = read_file_to_buffer("nonexistent_file.txt", &size);
    assert(buffer == NULL);
    printf("test_read_nonexistent_file passed.\n");
}

void test_null_file_size() {
    printf("Running test_null_file_size...\n");
    // Ensure it doesn't crash
    unsigned char* buffer = read_file_to_buffer("somefile", NULL);
    assert(buffer == NULL);
    printf("test_null_file_size passed.\n");
}

void test_read_large_file() {
    printf("Running test_read_large_file...\n");
    const char* filename = "large_file.bin";
    // Create a sparse file of 21MB
    FILE* f = fopen(filename, "wb");
    if (!f) {
        printf("Skipping test_read_large_file (fopen failed)\n");
        return;
    }
    if (fseek(f, 21 * 1024 * 1024, SEEK_SET) != 0) {
        printf("Skipping test_read_large_file (fseek failed)\n");
        fclose(f);
        return;
    }
    fputc(0, f);
    fclose(f);

    size_t size = 0;
    unsigned char* buffer = read_file_to_buffer(filename, &size);
    assert(buffer == NULL); // Should fail due to size limit
    remove(filename);
    printf("test_read_large_file passed.\n");
}

void test_base64_encode() {
    printf("Running test_base64_encode...\n");
    const char* input = "Hello";
    const char* expected = "SGVsbG8=";
    char* encoded = base64_encode((const unsigned char*)input, strlen(input));
    assert(encoded != NULL);
    assert(strcmp(encoded, expected) == 0);
    free(encoded);
    printf("test_base64_encode passed.\n");
}

int main() {
    test_read_existing_file();
    test_read_empty_file();
    test_read_nonexistent_file();
    test_null_file_size();
    test_read_large_file();
    test_base64_encode();
    printf("All tests passed!\n");
    return 0;
}
