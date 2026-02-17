#include "../utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

void test_read_existing_file() {
    const char* filename = "test_file.txt";
    const char* content = "Hello World";
    FILE* f = fopen(filename, "wb");
    assert(f != NULL);
    fwrite(content, 1, strlen(content), f);
    fclose(f);

    size_t size;
    unsigned char* buffer = read_file_to_buffer(filename, &size);
    assert(buffer != NULL);
    assert(size == strlen(content));
    assert(memcmp(buffer, content, size) == 0);
    free(buffer);
    remove(filename);
    printf("test_read_existing_file passed.\n");
}

void test_read_empty_file() {
    const char* filename = "empty_file.txt";
    FILE* f = fopen(filename, "wb");
    assert(f != NULL);
    fclose(f);

    size_t size;
    unsigned char* buffer = read_file_to_buffer(filename, &size);
    assert(buffer != NULL);
    assert(size == 0);
    // Our implementation allocates 1 byte and sets buffer[0] = '\0'
    assert(buffer[0] == '\0');

    free(buffer);
    remove(filename);
    printf("test_read_empty_file passed.\n");
}

void test_read_nonexistent_file() {
    const char* filename = "nonexistent_file.txt";
    size_t size;
    unsigned char* buffer = read_file_to_buffer(filename, &size);
    assert(buffer == NULL);
    printf("test_read_nonexistent_file passed.\n");
}

void test_read_large_file() {
    const char* filename = "large_file.bin";
    FILE* f = fopen(filename, "wb");
    if (f) {
        // Create a sparse file > 20MB
        fseek(f, 21 * 1024 * 1024, SEEK_SET);
        fputc('\0', f);
        fclose(f);

        size_t size;
        unsigned char* buffer = read_file_to_buffer(filename, &size);
        assert(buffer == NULL); // Should fail due to size limit
        remove(filename);
        printf("test_read_large_file passed.\n");
    } else {
        fprintf(stderr, "Warning: Could not create large file for testing.\n");
    }
}

void test_base64_encode() {
    const char* input = "Hello";
    const char* expected = "SGVsbG8=";
    char* encoded = base64_encode((const unsigned char*)input, strlen(input));
    assert(encoded != NULL);
    assert(strcmp(encoded, expected) == 0);
    free(encoded);

    const char* input2 = "Hello World";
    const char* expected2 = "SGVsbG8gV29ybGQ=";
    char* encoded2 = base64_encode((const unsigned char*)input2, strlen(input2));
    assert(encoded2 != NULL);
    assert(strcmp(encoded2, expected2) == 0);
    free(encoded2);

    printf("test_base64_encode passed.\n");
}

int main() {
    test_read_existing_file();
    test_read_empty_file();
    test_read_nonexistent_file();
    test_read_large_file();
    test_base64_encode();
    printf("All tests passed!\n");
    return 0;
}
