#include "buffer_utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int tests_run = 0;
int tests_failed = 0;

#define ASSERT_STR_EQ(actual, expected, msg) \
    tests_run++; \
    if (strcmp(actual, expected) != 0) { \
        fprintf(stderr, "FAIL: %s\n  Expected: %s\n  Actual:   %s\n", msg, expected, actual); \
        tests_failed++; \
    } else { \
        printf("PASS: %s\n", msg); \
    }

void test_base64_empty() {
    char *encoded = base64_encode((unsigned char*)"", 0);
    ASSERT_STR_EQ(encoded, "", "Empty string");
    free(encoded);
}

void test_base64_padding_0() {
    char *encoded = base64_encode((unsigned char*)"abc", 3);
    ASSERT_STR_EQ(encoded, "YWJj", "Length 3 (no padding)");
    free(encoded);
}

void test_base64_padding_1() {
    char *encoded = base64_encode((unsigned char*)"abcd", 4);
    ASSERT_STR_EQ(encoded, "YWJjZA==", "Length 4 (padding =)");
    free(encoded);
}

void test_base64_padding_2() {
    char *encoded = base64_encode((unsigned char*)"ab", 2);
    ASSERT_STR_EQ(encoded, "YWI=", "Length 2 (padding =)");
    // Wait, "ab" is 2 bytes. 2 * 8 = 16 bits. 16 / 6 = 2 remainder 4.
    // 'a' = 0x61, 'b' = 0x62. 0x61 0x62 0x00 -> 01100001 01100010 00000000
    // 011000 | 010110 | 001000 | 000000
    // 24 | 22 | 8 | 0
    // Y | W | I | A -> YWI=
    // Let's check: Y=24, W=22, I=8. Correct.
    // Wait, "ab" length 2.
    // octet_a = 'a', octet_b = 'b', octet_c = 0
    // triple = (0x61 << 16) + (0x62 << 8) + 0 = 0x616200
    // indices:
    // (0x616200 >> 18) & 0x3F = 0x616200 / 0x40000 = 0x18 = 24 (Y)
    // (0x616200 >> 12) & 0x3F = (0x616200 / 0x1000) & 0x3F = 0x616 & 0x3F = 1558 & 63 = 22 (W)
    // (0x616200 >> 6) & 0x3F = (0x616200 / 0x40) & 0x3F = 0x1858 & 0x3F = 6232 & 63 = 24 (Y)
    // Wait... 0x616200 -> 0110 0001 0110 0010 0000 0000
    // 011000 (24=Y) 010110 (22=W) 001000 (8=I) 000000 (0=A)
    // So "YWIA" but then padding replaces last one: "YWI="
    // Correct.

    // Test length 1
    encoded = base64_encode((unsigned char*)"a", 1);
    // 0x61 00 00 -> 011000 010000 000000 000000
    // 24 (Y) 16 (Q) 0 (A) 0 (A)
    // "YQAA" -> "YQ=="
    ASSERT_STR_EQ(encoded, "YQ==", "Length 1 (padding ==)");
    free(encoded);
}

void test_base64_binary() {
    unsigned char data[] = {0x00, 0xFF, 0x88, 0x44};
    // 0x00FF88 -> 00000000 11111111 10001000
    // 000000 (0=A) 001111 (15=P) 111110 (62=+) 001000 (8=I)
    // 0x44 00 00 -> 01000100 00000000 00000000
    // 010001 (17=R) 000000 (0=A) 000000 (0=A) 000000 (0=A)
    // "AP+IRAAA" -> "AP+IRA=="
    char *encoded = base64_encode(data, 4);
    ASSERT_STR_EQ(encoded, "AP+IRA==", "Binary data");
    free(encoded);
}

int main() {
    test_base64_empty();
    test_base64_padding_0();
    test_base64_padding_1();
    test_base64_padding_2();
    test_base64_binary();

    printf("\nTests run: %d, Tests failed: %d\n", tests_run, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
