#include "buffer_utils.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

void test_encode(const char *input, const char *expected) {
    char *encoded = base64_encode((const unsigned char *)input, strlen(input));
    assert(encoded != NULL);
    if (strcmp(encoded, expected) != 0) {
        fprintf(stderr, "FAIL: Input '%s', Expected '%s', Got '%s'\n", input, expected, encoded);
        exit(1);
    }
    printf("PASS: Input '%s' -> '%s'\n", input, encoded);
    free(encoded);
}

void test_binary() {
    // Binary data with null byte
    unsigned char data[] = { 0x00, 0x01, 0x02, 0xFF };
    char *encoded = base64_encode(data, sizeof(data));
    const char *expected = "AAEC/w==";
    assert(encoded != NULL);
    if (strcmp(encoded, expected) != 0) {
        fprintf(stderr, "FAIL: Binary test, Expected '%s', Got '%s'\n", expected, encoded);
        exit(1);
    }
    printf("PASS: Binary test -> '%s'\n", encoded);
    free(encoded);
}

int main() {
    printf("Running Base64 Tests...\n");
    test_encode("Hello World", "SGVsbG8gV29ybGQ=");
    test_encode("", "");
    test_encode("M", "TQ==");
    test_encode("Ma", "TWE=");
    test_encode("Motif", "TW90aWY=");
    test_binary();
    printf("All tests passed.\n");
    return 0;
}
