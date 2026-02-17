#include "../utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>

void test_generate_system_prompt() {
    char buffer[4096];
    char dateStr[80];
    time_t now = time(0);
    struct tm* ltm = localtime(&now);
    strftime(dateStr, sizeof(dateStr), "%A, %B %d, %Y", ltm);

    // Case 1: Default prompt only
    generate_system_prompt(buffer, sizeof(buffer), NULL, true);
    assert(strstr(buffer, "You are MotifGPT") != NULL);
    assert(strstr(buffer, dateStr) != NULL);

    // Case 2: Custom prompt only
    generate_system_prompt(buffer, sizeof(buffer), "Custom Prompt", false);
    assert(strcmp(buffer, "Custom Prompt") == 0);

    // Case 3: Appended prompt
    generate_system_prompt(buffer, sizeof(buffer), "Custom Prompt", true);
    assert(strstr(buffer, "You are MotifGPT") != NULL);
    assert(strstr(buffer, dateStr) != NULL);
    assert(strstr(buffer, "Custom Prompt") != NULL);

    printf("test_generate_system_prompt passed.\n");
}

void test_base64_encode() {
    const char* input = "Hello World";
    char* encoded = base64_encode((unsigned char*)input, strlen(input));
    // "Hello World" -> "SGVsbG8gV29ybGQ="
    assert(strcmp(encoded, "SGVsbG8gV29ybGQ=") == 0);
    free(encoded);

    const char* input2 = "A";
    char* encoded2 = base64_encode((unsigned char*)input2, strlen(input2));
    // "A" -> "QQ=="
    assert(strcmp(encoded2, "QQ==") == 0);
    free(encoded2);

    printf("test_base64_encode passed.\n");
}

void test_read_file_to_buffer() {
    const char* filename = "test_file.txt";
    const char* content = "Test Content";
    FILE* f = fopen(filename, "wb");
    fwrite(content, 1, strlen(content), f);
    fclose(f);

    size_t size;
    unsigned char* buffer = read_file_to_buffer(filename, &size);
    assert(buffer != NULL);
    assert(size == strlen(content));
    assert(memcmp(buffer, content, size) == 0);
    free(buffer);

    unlink(filename);

    // Test empty file
    const char* empty_filename = "empty_file.txt";
    f = fopen(empty_filename, "wb");
    fclose(f);

    buffer = read_file_to_buffer(empty_filename, &size);
    assert(buffer != NULL);
    assert(size == 0);
    // Since we malloc(1) for empty file, buffer is valid pointer
    free(buffer);

    unlink(empty_filename);

    printf("test_read_file_to_buffer passed.\n");
}

int main() {
    test_generate_system_prompt();
    test_base64_encode();
    test_read_file_to_buffer();
    return 0;
}
