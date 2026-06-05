#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Mock structures based on motifgpt.c usage
typedef enum {
    DP_ROLE_USER,
    DP_ROLE_ASSISTANT
} dp_message_role_t;

typedef enum {
    DP_CONTENT_PART_TEXT,
    DP_CONTENT_PART_IMAGE_BASE64
} dp_content_part_type_t;

typedef struct {
    dp_content_part_type_t type;
    char *text;
} dp_content_part_t;

typedef struct {
    dp_message_role_t role;
    size_t num_parts;
    dp_content_part_t *parts;
} dp_message_t;

#define USER_NICKNAME "User"
#define ASSISTANT_NICKNAME "Assistant"
#define NUM_MESSAGES 1000
#define PARTS_PER_MESSAGE 200

dp_message_t chat_history[NUM_MESSAGES];
int chat_history_count = NUM_MESSAGES;

// Mock append_to_conversation
void append_to_conversation(const char* text) {
    // volatile to prevent optimization
    volatile size_t len = strlen(text);
    (void)len;
}

void setup_benchmark() {
    for (int i = 0; i < NUM_MESSAGES; i++) {
        chat_history[i].role = (i % 2 == 0) ? DP_ROLE_USER : DP_ROLE_ASSISTANT;
        chat_history[i].num_parts = PARTS_PER_MESSAGE;
        chat_history[i].parts = malloc(sizeof(dp_content_part_t) * PARTS_PER_MESSAGE);
        for (size_t j = 0; j < PARTS_PER_MESSAGE; j++) {
            chat_history[i].parts[j].type = DP_CONTENT_PART_TEXT;
            chat_history[i].parts[j].text = "word ";
        }
    }
}

void teardown_benchmark() {
    for (int i = 0; i < NUM_MESSAGES; i++) {
        free(chat_history[i].parts);
    }
}

void original_implementation() {
    for (int i = 0; i < chat_history_count; i++) {
        const char* nick = (chat_history[i].role == DP_ROLE_USER) ? USER_NICKNAME : ASSISTANT_NICKNAME;
        char line_buffer[8192];
        snprintf(line_buffer, sizeof(line_buffer), "%s: ", nick);

        for (size_t j = 0; j < chat_history[i].num_parts; j++) {
            dp_content_part_t* part = &chat_history[i].parts[j];
            if (part->type == DP_CONTENT_PART_TEXT) {
                strncat(line_buffer, part->text, sizeof(line_buffer) - strlen(line_buffer) - 1);
            } else if (part->type == DP_CONTENT_PART_IMAGE_BASE64) {
                strncat(line_buffer, " [Image Attached]", sizeof(line_buffer) - strlen(line_buffer) - 1);
            }
        }
        strncat(line_buffer, "\n", sizeof(line_buffer) - strlen(line_buffer) - 1);
        append_to_conversation(line_buffer);
    }
}

void optimized_implementation() {
    for (int i = 0; i < chat_history_count; i++) {
        const char* nick = (chat_history[i].role == DP_ROLE_USER) ? USER_NICKNAME : ASSISTANT_NICKNAME;
        char line_buffer[8192];
        size_t current_len = 0;
        size_t buffer_size = sizeof(line_buffer);

        int written = snprintf(line_buffer, buffer_size, "%s: ", nick);
        if (written < 0) written = 0;
        else if ((size_t)written >= buffer_size) written = buffer_size - 1;
        current_len += written;

        for (size_t j = 0; j < chat_history[i].num_parts; j++) {
            dp_content_part_t* part = &chat_history[i].parts[j];
            const char* text_to_append = NULL;

            if (part->type == DP_CONTENT_PART_TEXT) {
                text_to_append = part->text;
            } else if (part->type == DP_CONTENT_PART_IMAGE_BASE64) {
                text_to_append = " [Image Attached]";
            }

            if (text_to_append) {
                // Using snprintf to safely append
                size_t remaining = buffer_size - current_len;
                if (remaining > 1) {
                    int ret = snprintf(line_buffer + current_len, remaining, "%s", text_to_append);
                    if (ret > 0) {
                        if ((size_t)ret >= remaining) {
                             current_len += (remaining - 1);
                        } else {
                             current_len += ret;
                        }
                    }
                }
            }
        }

        if (current_len < buffer_size - 1) {
            line_buffer[current_len++] = '\n';
            line_buffer[current_len] = '\0';
        }
        append_to_conversation(line_buffer);
    }
}

int main() {
    setup_benchmark();

    clock_t start = clock();
    original_implementation();
    clock_t end = clock();
    double cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Original Implementation: %f seconds\n", cpu_time_used);

    start = clock();
    optimized_implementation();
    end = clock();
    double cpu_time_used_opt = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Optimized Implementation: %f seconds\n", cpu_time_used_opt);

    if (cpu_time_used_opt > 0) {
        printf("Speedup: %.2fx\n", cpu_time_used / cpu_time_used_opt);
    } else {
        printf("Speedup: (too fast to measure)\n");
    }

    teardown_benchmark();
    return 0;
}
