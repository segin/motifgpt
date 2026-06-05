#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

void generate_system_prompt(char *buffer, size_t buffer_size, const char *custom_prompt, bool append_default) {
    char default_prompt[512];
    time_t now = time(0);
    struct tm* ltm = localtime(&now);
    char dateStr[80];
    strftime(dateStr, sizeof(dateStr), "%A, %B %d, %Y", ltm);
    snprintf(default_prompt, sizeof(default_prompt),
             "- You are MotifGPT, an assistant whose client program runs on UNIX with the Motif toolkit.\n"
             "- The current date is %s.\n"
             "- The user's environment does not format Markdown. Do not produce any Markdown unless the user explicitly requests it.",
             dateStr);

    if (custom_prompt == NULL || strlen(custom_prompt) == 0) {
        strncpy(buffer, default_prompt, buffer_size - 1);
        buffer[buffer_size - 1] = '\0';
    } else {
        if (append_default) {
            snprintf(buffer, buffer_size,
                     "%s\n\n%s", default_prompt, custom_prompt);
        } else {
            strncpy(buffer, custom_prompt, buffer_size - 1);
            buffer[buffer_size - 1] = '\0';
        }
    }
}

unsigned char* read_file_to_buffer(const char* filename, size_t* file_size) {
    FILE* f = fopen(filename, "rb");
    if (!f) { perror("fopen read_file"); return NULL; }
    fseek(f, 0, SEEK_END); long size = ftell(f);
    if (size < 0 || size > MAX_FILE_SIZE_BYTES) {
        fclose(f); fprintf(stderr, "File too large (max %dMB) or ftell error.\n", (int)(MAX_FILE_SIZE_BYTES / (1024 * 1024))); return NULL;
    }
    *file_size = (size_t)size; fseek(f, 0, SEEK_SET);
    unsigned char* buffer = malloc(*file_size ? *file_size : 1);
    if (!buffer) { fclose(f); perror("malloc read_file"); return NULL; }
    if (*file_size > 0) {
        if (fread(buffer, 1, *file_size, f) != *file_size) {
            fclose(f); free(buffer); fprintf(stderr, "fread error.\n"); return NULL;
        }
    }
    fclose(f); return buffer;
}

const char* get_image_mime_type(const unsigned char* buffer, size_t len) {
    if (!buffer || len == 0) return NULL;

    // PNG: 89 50 4E 47 0D 0A 1A 0A
    if (len >= 8 && memcmp(buffer, "\x89PNG\r\n\x1a\n", 8) == 0) {
        return "image/png";
    }

    // JPEG: FF D8
    if (len >= 2 && memcmp(buffer, "\xff\xd8", 2) == 0) {
        return "image/jpeg";
    }

    // GIF: GIF87a or GIF89a
    if (len >= 6 && (memcmp(buffer, "GIF87a", 6) == 0 || memcmp(buffer, "GIF89a", 6) == 0)) {
        return "image/gif";
    }

    return NULL;
}

char* base64_encode(const unsigned char *data, size_t input_length) {
    const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t output_length = 4 * ((input_length + 2) / 3);
    char *encoded_data = malloc(output_length + 1);
    if (!encoded_data) { perror("malloc base64"); return NULL; }
    for (size_t i = 0, j = 0; i < input_length;) {
        uint32_t octet_a = i < input_length ? data[i++] : 0;
        uint32_t octet_b = i < input_length ? data[i++] : 0;
        uint32_t octet_c = i < input_length ? data[i++] : 0;
        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;
        encoded_data[j++] = base64_chars[(triple >> 18) & 0x3F];
        encoded_data[j++] = base64_chars[(triple >> 12) & 0x3F];
        encoded_data[j++] = base64_chars[(triple >> 6) & 0x3F];
        encoded_data[j++] = base64_chars[(triple >> 0) & 0x3F];
    }

    size_t padding = (3 - input_length % 3) % 3;
    for (size_t i = 0; i < padding; i++) encoded_data[output_length - 1 - i] = '=';
    encoded_data[output_length] = '\0';
    return encoded_data;
}
