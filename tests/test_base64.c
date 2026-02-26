#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../utils.h"

int main() {
    printf("Running base64_encode tests...\n");

    // Test case 1: Empty string
    char *result = base64_encode((unsigned char*)"", 0);
    if (result == NULL) {
        fprintf(stderr, "base64_encode returned NULL for empty input\n");
        return 1;
    }
    assert(strcmp(result, "") == 0);
    free(result);

    // Test case 2: 'f' -> 'Zg=='
    result = base64_encode((unsigned char*)"f", 1);
    assert(strcmp(result, "Zg==") == 0);
    free(result);

    // Test case 3: 'fo' -> 'Zm8='
    result = base64_encode((unsigned char*)"fo", 2);
    assert(strcmp(result, "Zm8=") == 0);
    free(result);

    // Test case 4: 'foo' -> 'Zm9v'
    result = base64_encode((unsigned char*)"foo", 3);
    assert(strcmp(result, "Zm9v") == 0);
    free(result);

    // Test case 5: 'foob' -> 'Zm9vYg=='
    result = base64_encode((unsigned char*)"foob", 4);
    assert(strcmp(result, "Zm9vYg==") == 0);
    free(result);

    // Test case 6: 'fooba' -> 'Zm9vYmE='
    result = base64_encode((unsigned char*)"fooba", 5);
    assert(strcmp(result, "Zm9vYmE=") == 0);
    free(result);

    // Test case 7: 'foobar' -> 'Zm9vYmFy'
    result = base64_encode((unsigned char*)"foobar", 6);
    assert(strcmp(result, "Zm9vYmFy") == 0);
    free(result);

    printf("All base64_encode tests passed!\n");
    return 0;
}
