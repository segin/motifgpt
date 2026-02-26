#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../utils.h"

void test_base64_encode() {
    printf("Testing base64_encode...\n");

    struct {
        const char* input;
        const char* expected;
    } cases[] = {
        {"", ""},
        {"f", "Zg=="},
        {"fo", "Zm8="},
        {"foo", "Zm9v"},
        {"foob", "Zm9vYg=="},
        {"fooba", "Zm9vYmE="},
        {"foobar", "Zm9vYmFy"},
        {NULL, NULL}
    };

    for (int i = 0; cases[i].input != NULL; i++) {
        char* encoded = base64_encode((const unsigned char*)cases[i].input, strlen(cases[i].input));
        assert(encoded != NULL);
        if (strcmp(encoded, cases[i].expected) != 0) {
            fprintf(stderr, "Case %d failed: input='%s', expected='%s', got='%s'\n", i, cases[i].input, cases[i].expected, encoded);
            exit(1);
        }
        free(encoded);
    }
    printf("base64_encode tests passed!\n");
}

void test_read_file_to_buffer() {
    printf("Testing read_file_to_buffer...\n");

    // 1. Test reading a normal file
    const char* test_file = "test_normal.txt";
    const char* content = "Hello, Base64!";
    FILE* f = fopen(test_file, "wb");
    fwrite(content, 1, strlen(content), f);
    fclose(f);

    size_t size;
    unsigned char* buffer = read_file_to_buffer(test_file, &size);
    assert(buffer != NULL);
    assert(size == strlen(content));
    assert(memcmp(buffer, content, size) == 0);
    free(buffer);
    remove(test_file);

    // 2. Test reading an empty file
    const char* empty_file = "test_empty.txt";
    f = fopen(empty_file, "wb");
    fclose(f);

    buffer = read_file_to_buffer(empty_file, &size);
    assert(buffer != NULL);
    assert(size == 0);
    free(buffer);
    remove(empty_file);

    // 3. Test non-existent file
    buffer = read_file_to_buffer("non_existent.txt", &size);
    assert(buffer == NULL);

    // 4. Test file too large (simulated by creating a file > 20MB)
    // Actually, creating a 20MB+ file might be slow and consume space.
    // Let's create a 21MB file to test the limit.
    const char* large_file = "test_large.txt";
    f = fopen(large_file, "wb");
    if (f) {
        fseek(f, 20 * 1024 * 1024 + 1024, SEEK_SET);
        fputc(0, f);
        fclose(f);

        buffer = read_file_to_buffer(large_file, &size);
        assert(buffer == NULL);
        remove(large_file);
    }

    printf("read_file_to_buffer tests passed!\n");
}

int main() {
    test_base64_encode();
    test_read_file_to_buffer();
    printf("All tests passed successfully!\n");
    return 0;
}
