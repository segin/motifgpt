#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// --- Helper Functions ---

// Creates a file with specific bytes
void create_test_file(const char *filename, const unsigned char *bytes, size_t len) {
    FILE *f = fopen(filename, "wb");
    if (!f) {
        perror("fopen");
        exit(1);
    }
    fwrite(bytes, 1, len, f);
    fclose(f);
}

// Function to test (will be copied to motifgpt.c later)
const char* get_image_mime_type(const char* filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return NULL;

    unsigned char header[8];
    size_t read_bytes = fread(header, 1, 8, f);
    fclose(f);

    if (read_bytes < 8) return NULL; // File too small for reliable detection

    // PNG: 89 50 4E 47 0D 0A 1A 0A
    if (header[0] == 0x89 && header[1] == 0x50 && header[2] == 0x4E && header[3] == 0x47 &&
        header[4] == 0x0D && header[5] == 0x0A && header[6] == 0x1A && header[7] == 0x0A) {
        return "image/png";
    }

    // JPEG: FF D8 FF
    if (header[0] == 0xFF && header[1] == 0xD8 && header[2] == 0xFF) {
        return "image/jpeg";
    }

    // GIF: "GIF87a" or "GIF89a" -> 47 49 46 38 37 61 / 39 61
    if (header[0] == 'G' && header[1] == 'I' && header[2] == 'F' && header[3] == '8' &&
        (header[4] == '7' || header[4] == '9') && header[5] == 'a') {
        return "image/gif";
    }

    return NULL;
}

int main() {
    int failed = 0;

    // Test 1: Valid PNG
    unsigned char png_magic[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    create_test_file("valid.png", png_magic, 8);
    const char *mime = get_image_mime_type("valid.png");
    if (mime && strcmp(mime, "image/png") == 0) {
        printf("PASS: valid.png detected as %s\n", mime);
    } else {
        printf("FAIL: valid.png detected as %s\n", mime ? mime : "NULL");
        failed++;
    }
    remove("valid.png");

    // Test 2: Valid JPEG
    unsigned char jpg_magic[] = {0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46};
    create_test_file("valid.jpg", jpg_magic, 8);
    mime = get_image_mime_type("valid.jpg");
    if (mime && strcmp(mime, "image/jpeg") == 0) {
        printf("PASS: valid.jpg detected as %s\n", mime);
    } else {
        printf("FAIL: valid.jpg detected as %s\n", mime ? mime : "NULL");
        failed++;
    }
    remove("valid.jpg");

    // Test 3: Valid GIF89a
    unsigned char gif89_magic[] = {'G', 'I', 'F', '8', '9', 'a', 0x01, 0x00};
    create_test_file("valid.gif", gif89_magic, 8);
    mime = get_image_mime_type("valid.gif");
    if (mime && strcmp(mime, "image/gif") == 0) {
        printf("PASS: valid.gif detected as %s\n", mime);
    } else {
        printf("FAIL: valid.gif detected as %s\n", mime ? mime : "NULL");
        failed++;
    }
    remove("valid.gif");

    // Test 4: Fake PNG (text file renamed)
    const char *fake_content = "This is not a PNG file.";
    create_test_file("fake.png", (unsigned char*)fake_content, strlen(fake_content));
    mime = get_image_mime_type("fake.png");
    if (mime == NULL) {
        printf("PASS: fake.png correctly rejected (NULL)\n");
    } else {
        printf("FAIL: fake.png accepted as %s\n", mime);
        failed++;
    }
    remove("fake.png");

    // Test 5: Short file
    const char *short_content = "123";
    create_test_file("short_file", (unsigned char*)short_content, 3);
    mime = get_image_mime_type("short_file");
    if (mime == NULL) {
        printf("PASS: short_file correctly rejected (NULL)\n");
    } else {
        printf("FAIL: short_file accepted as %s\n", mime);
        failed++;
    }
    remove("short_file");

    // Test 6: Non-existent file
    mime = get_image_mime_type("non_existent_file.xyz");
    if (mime == NULL) {
        printf("PASS: non_existent_file correctly rejected (NULL)\n");
    } else {
        printf("FAIL: non_existent_file accepted as %s\n", mime);
        failed++;
    }

    if (failed == 0) {
        printf("All tests passed!\n");
        return 0;
    } else {
        printf("%d tests failed.\n", failed);
        return 1;
    }
}
