#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>

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
    char *image_mime_type;
    char *image_base64_data;
} dp_content_part_t;

typedef struct {
    dp_message_role_t role;
    size_t num_parts;
    dp_content_part_t *parts;
} dp_message_t;

#define USER_NICKNAME "User"
#define ASSISTANT_NICKNAME "Assistant"

dp_message_t *chat_history = NULL;
int chat_history_count = 0;

void setup_history(int num_messages, int parts_per_message) {
    chat_history_count = num_messages;
    chat_history = malloc(sizeof(dp_message_t) * num_messages);
    for (int i = 0; i < num_messages; i++) {
        chat_history[i].role = (i % 2 == 0) ? DP_ROLE_USER : DP_ROLE_ASSISTANT;
        chat_history[i].num_parts = parts_per_message;
        chat_history[i].parts = malloc(sizeof(dp_content_part_t) * parts_per_message);
        for (int j = 0; j < parts_per_message; j++) {
            chat_history[i].parts[j].type = DP_CONTENT_PART_TEXT;
            chat_history[i].parts[j].text = strdup("Lorem ipsum dolor sit amet, consectetur adipiscing elit. ");
        }
    }
}

void teardown_history() {
    for (int i = 0; i < chat_history_count; i++) {
        for (int j = 0; j < chat_history[i].num_parts; j++) {
            free(chat_history[i].parts[j].text);
        }
        free(chat_history[i].parts);
    }
    free(chat_history);
}

void current_render_all_history() {
    size_t total_len = 0;
    for (int i = 0; i < chat_history_count; i++) {
        const char* nick = (chat_history[i].role == DP_ROLE_USER) ? USER_NICKNAME : ASSISTANT_NICKNAME;
        total_len += strlen(nick) + 2;

        for (size_t j = 0; j < chat_history[i].num_parts; j++) {
            dp_content_part_t* part = &chat_history[i].parts[j];
            if (part->type == DP_CONTENT_PART_TEXT) {
                if (part->text) total_len += strlen(part->text);
            } else if (part->type == DP_CONTENT_PART_IMAGE_BASE64) {
                total_len += strlen(" [Image Attached]");
            }
        }
        total_len += 1;
    }

    char* full_history = malloc(total_len + 1);
    if (!full_history) return;

    char* current_pos = full_history;
    for (int i = 0; i < chat_history_count; i++) {
        const char* nick = (chat_history[i].role == DP_ROLE_USER) ? USER_NICKNAME : ASSISTANT_NICKNAME;

        size_t nick_len = strlen(nick);
        memcpy(current_pos, nick, nick_len);
        current_pos += nick_len;
        memcpy(current_pos, ": ", 2);
        current_pos += 2;

        for (size_t j = 0; j < chat_history[i].num_parts; j++) {
            dp_content_part_t* part = &chat_history[i].parts[j];
            if (part->type == DP_CONTENT_PART_TEXT && part->text) {
                size_t text_len = strlen(part->text);
                memcpy(current_pos, part->text, text_len);
                current_pos += text_len;
            } else if (part->type == DP_CONTENT_PART_IMAGE_BASE64) {
                 const char* img_msg = " [Image Attached]";
                 size_t img_len = strlen(img_msg);
                 memcpy(current_pos, img_msg, img_len);
                 current_pos += img_len;
            }
        }
        *current_pos = '\n';
        current_pos++;
    }
    *current_pos = '\0';

    volatile char first = full_history[0];
    (void)first;
    free(full_history);
}

void optimized_render_all_history() {
    static const size_t user_nick_len = sizeof(USER_NICKNAME) - 1;
    static const size_t assistant_nick_len = sizeof(ASSISTANT_NICKNAME) - 1;
    static const char img_msg[] = " [Image Attached]";
    static const size_t img_msg_len = sizeof(img_msg) - 1;

    size_t total_len = 0;
    size_t total_parts = 0;
    for (int i = 0; i < chat_history_count; i++) {
        total_parts += chat_history[i].num_parts;
    }

    size_t *part_lengths = NULL;
    if (total_parts > 0) {
        part_lengths = malloc(total_parts * sizeof(size_t));
        if (!part_lengths) return;
    }

    size_t part_idx = 0;
    for (int i = 0; i < chat_history_count; i++) {
        size_t nick_len = (chat_history[i].role == DP_ROLE_USER) ? user_nick_len : assistant_nick_len;
        total_len += nick_len + 2;

        for (size_t j = 0; j < chat_history[i].num_parts; j++) {
            dp_content_part_t* part = &chat_history[i].parts[j];
            if (part->type == DP_CONTENT_PART_TEXT) {
                if (part->text) {
                    size_t len = strlen(part->text);
                    part_lengths[part_idx] = len;
                    total_len += len;
                } else {
                    part_lengths[part_idx] = 0;
                }
            } else if (part->type == DP_CONTENT_PART_IMAGE_BASE64) {
                part_lengths[part_idx] = img_msg_len;
                total_len += img_msg_len;
            }
            part_idx++;
        }
        total_len += 1;
    }

    char* full_history = malloc(total_len + 1);
    if (!full_history) {
        free(part_lengths);
        return;
    }

    char* current_pos = full_history;
    part_idx = 0;
    for (int i = 0; i < chat_history_count; i++) {
        const char* nick = (chat_history[i].role == DP_ROLE_USER) ? USER_NICKNAME : ASSISTANT_NICKNAME;
        size_t nick_len = (chat_history[i].role == DP_ROLE_USER) ? user_nick_len : assistant_nick_len;

        memcpy(current_pos, nick, nick_len);
        current_pos += nick_len;
        memcpy(current_pos, ": ", 2);
        current_pos += 2;

        for (size_t j = 0; j < chat_history[i].num_parts; j++) {
            dp_content_part_t* part = &chat_history[i].parts[j];
            size_t len = (part_lengths != NULL) ? part_lengths[part_idx] : 0;
            if (part->type == DP_CONTENT_PART_TEXT && part->text) {
                memcpy(current_pos, part->text, len);
                current_pos += len;
            } else if (part->type == DP_CONTENT_PART_IMAGE_BASE64) {
                 memcpy(current_pos, img_msg, len);
                 current_pos += len;
            }
            part_idx++;
        }
        *current_pos = '\n';
        current_pos++;
    }
    *current_pos = '\0';

    volatile char first = full_history[0];
    (void)first;
    free(full_history);
    free(part_lengths);
}

int main() {
    const int NUM_MESSAGES = 1000;
    const int PARTS_PER_MESSAGE = 10;
    const int ITERATIONS = 500;

    printf("Setting up history with %d messages, %d parts each...\n", NUM_MESSAGES, PARTS_PER_MESSAGE);
    setup_history(NUM_MESSAGES, PARTS_PER_MESSAGE);

    printf("Running current implementation for %d iterations...\n", ITERATIONS);
    clock_t start = clock();
    for (int i = 0; i < ITERATIONS; i++) {
        current_render_all_history();
    }
    clock_t end = clock();
    double cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Current Implementation total time: %f seconds\n", cpu_time_used);
    printf("Average time per iteration: %f ms\n", (cpu_time_used * 1000) / ITERATIONS);

    printf("\nRunning optimized implementation for %d iterations...\n", ITERATIONS);
    start = clock();
    for (int i = 0; i < ITERATIONS; i++) {
        optimized_render_all_history();
    }
    end = clock();
    double cpu_time_used_opt = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Optimized Implementation total time: %f seconds\n", cpu_time_used_opt);
    printf("Average time per iteration: %f ms\n", (cpu_time_used_opt * 1000) / ITERATIONS);

    if (cpu_time_used_opt > 0) {
        printf("\nSpeedup: %.2fx\n", cpu_time_used / cpu_time_used_opt);
    }

    teardown_history();
    return 0;
}
