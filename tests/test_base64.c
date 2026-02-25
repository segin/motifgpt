#include "buffer_utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

void test_base64() {
    const unsigned char input[] = "Hello World";
    char *encoded = base64_encode(input, strlen((char*)input));
    // "Hello World" -> "SGVsbG8gV29ybGQ="
    assert(strcmp(encoded, "SGVsbG8gV29ybGQ=") == 0);
    free(encoded);

    const unsigned char input2[] = "";
    char *encoded2 = base64_encode(input2, 0);
    assert(strcmp(encoded2, "") == 0);
    free(encoded2);
}

int main() {
    test_base64();
    printf("test_base64 passed!\n");
    return 0;
}
