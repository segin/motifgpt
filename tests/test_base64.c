#include "../buffer_utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

void test_base64_encode() {
    printf("Running test_base64_encode...\n");

    struct {
        const char *input;
        const char *expected;
    } cases[] = {
        {"", ""},
        {"f", "Zg=="},
        {"fo", "Zm8="},
        {"foo", "Zm9v"},
        {"foob", "Zm9vYg=="},
        {"fooba", "Zm9vYmE="},
        {"foobar", "Zm9vYmFy"},
        {"Hello, World!", "SGVsbG8sIFdvcmxkIQ=="}
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        char *encoded = base64_encode((const unsigned char *)cases[i].input, strlen(cases[i].input));
        if (strcmp(encoded, cases[i].expected) != 0) {
            fprintf(stderr, "Test case %zu failed: input='%s', expected='%s', got='%s'\n",
                    i, cases[i].input, cases[i].expected, encoded);
            free(encoded);
            exit(1);
        }
        printf("Test case %zu passed: '%s' -> '%s'\n", i, cases[i].input, encoded);
        free(encoded);
    }

    printf("test_base64_encode passed!\n");
}

int main() {
    test_base64_encode();
    return 0;
}
