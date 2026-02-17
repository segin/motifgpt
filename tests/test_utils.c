#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../utils.h"

void test_get_image_mime_type() {
    // PNG
    unsigned char png_header[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    assert(strcmp(get_image_mime_type(png_header, sizeof(png_header)), "image/png") == 0);

    // JPEG
    unsigned char jpeg_header[] = {0xFF, 0xD8, 0xFF};
    assert(strcmp(get_image_mime_type(jpeg_header, sizeof(jpeg_header)), "image/jpeg") == 0);

    unsigned char jpeg_header_short[] = {0xFF, 0xD8};
    assert(strcmp(get_image_mime_type(jpeg_header_short, sizeof(jpeg_header_short)), "image/jpeg") == 0);

    // GIF
    unsigned char gif87a_header[] = {'G', 'I', 'F', '8', '7', 'a'};
    assert(strcmp(get_image_mime_type(gif87a_header, sizeof(gif87a_header)), "image/gif") == 0);

    unsigned char gif89a_header[] = {'G', 'I', 'F', '8', '9', 'a'};
    assert(strcmp(get_image_mime_type(gif89a_header, sizeof(gif89a_header)), "image/gif") == 0);

    // Invalid
    unsigned char invalid_header[] = {0x00, 0x00, 0x00};
    assert(get_image_mime_type(invalid_header, sizeof(invalid_header)) == NULL);

    // Too short
    unsigned char short_header[] = {0xFF};
    assert(get_image_mime_type(short_header, sizeof(short_header)) == NULL);

    printf("test_get_image_mime_type passed.\n");
}

void test_base64_encode() {
    const char *data = "Hello World";
    char *encoded = base64_encode((const unsigned char*)data, strlen(data));
    assert(encoded != NULL);
    assert(strcmp(encoded, "SGVsbG8gV29ybGQ=") == 0);
    free(encoded);

    const char *data2 = "A";
    encoded = base64_encode((const unsigned char*)data2, strlen(data2));
    assert(strcmp(encoded, "QQ==") == 0);
    free(encoded);

    const char *data3 = "AB";
    encoded = base64_encode((const unsigned char*)data3, strlen(data3));
    assert(strcmp(encoded, "QUI=") == 0);
    free(encoded);

    printf("test_base64_encode passed.\n");
}

int main() {
    test_get_image_mime_type();
    test_base64_encode();
    return 0;
}
