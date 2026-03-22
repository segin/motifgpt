#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <errno.h>

// Mocking disasterparty types
typedef struct { int x; } dp_context_t;
typedef struct { int role; } dp_message_t;
typedef struct {
    char *model;
    float temperature;
    int max_tokens;
    bool stream;
    dp_message_t *messages;
    size_t num_messages;
    char *system_prompt;
} dp_request_config_t;

typedef struct {
    dp_request_config_t config;
    char system_prompt_buffer[4096];
    char temp_history_filename[PATH_MAX];
} llm_thread_data_t;

// Mock globals
dp_message_t *chat_history = NULL;
int chat_history_count = 5;

// Mock functions
int dp_serialize_messages_to_fd(dp_message_t *messages, size_t count, int fd) {
    printf("Mock: Serializing %zu messages to fd %d\n", count, fd);
    if (write(fd, "mock data", 9) == 9) return 0;
    return -1;
}

int dp_serialize_messages_to_file(dp_message_t *messages, size_t count, const char *filename) {
    printf("Mock: Serializing %zu messages to %s\n", count, filename);
    // Create the file to simulate success
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd != -1) {
        int ret = dp_serialize_messages_to_fd(messages, count, fd);
        close(fd);
        return ret;
    }
    return -1;
}

int dp_deserialize_messages_from_file(const char *filename, dp_message_t **messages, size_t *count) {
    printf("Mock: Deserializing from %s\n", filename);
    if (access(filename, F_OK) != 0) return -1;
    *count = 5;
    *messages = malloc(sizeof(dp_message_t) * (*count));
    return 0;
}

void dp_free_messages(dp_message_t *messages, size_t count) {
    printf("Mock: Freeing messages\n");
}

void show_error_dialog(const char *msg) {
    printf("Mock Error Dialog: %s\n", msg);
}

// Logic extracted from start_llm_request
void test_start_logic(llm_thread_data_t *thread_data) {
    char temp_hist_template[PATH_MAX];
    const char *tmp_dir = getenv("TMPDIR");
    if (!tmp_dir) tmp_dir = "/tmp";
    snprintf(temp_hist_template, sizeof(temp_hist_template), "%s/motifgpt_hist_XXXXXX", tmp_dir);

    int fd = mkstemp(temp_hist_template);
    if (fd == -1) {
        perror("mkstemp history");
        return;
    }

    if (dp_serialize_messages_to_fd(chat_history, chat_history_count, fd) != 0) {
        close(fd);
        unlink(temp_hist_template);
        return;
    }
    close(fd);

    strncpy(thread_data->temp_history_filename, temp_hist_template, PATH_MAX - 1);
    thread_data->temp_history_filename[PATH_MAX - 1] = '\0';

    thread_data->config.messages = NULL;
    thread_data->config.num_messages = chat_history_count;
}

// Logic extracted from perform_llm_request_thread
void test_thread_logic(llm_thread_data_t *thread_data) {
    dp_message_t *messages = NULL;
    size_t num_messages = 0;

    if (strlen(thread_data->temp_history_filename) > 0) {
        if (dp_deserialize_messages_from_file(thread_data->temp_history_filename, &messages, &num_messages) != 0) {
             printf("Failed to deserialize\n");
             unlink(thread_data->temp_history_filename);
             return;
        }
        thread_data->config.messages = messages;
        thread_data->config.num_messages = num_messages;
    }

    printf("Thread has %zu messages\n", thread_data->config.num_messages);

    if (messages) {
        dp_free_messages(messages, num_messages);
        free(messages);
    }
    if (strlen(thread_data->temp_history_filename) > 0) {
        unlink(thread_data->temp_history_filename);
        printf("Unlinked %s\n", thread_data->temp_history_filename);
    }
}

int main() {
    llm_thread_data_t *data = malloc(sizeof(llm_thread_data_t));
    memset(data, 0, sizeof(llm_thread_data_t));

    printf("Running test_start_logic...\n");
    test_start_logic(data);

    if (strlen(data->temp_history_filename) == 0) {
        printf("FAIL: temp_history_filename empty\n");
        return 1;
    }
    if (data->config.messages != NULL) {
        printf("FAIL: messages not NULL\n");
        return 1;
    }

    printf("Temp file created: %s\n", data->temp_history_filename);

    printf("Running test_thread_logic...\n");
    test_thread_logic(data);

    if (access(data->temp_history_filename, F_OK) == 0) {
        printf("FAIL: Temp file still exists\n");
        return 1;
    }

    free(data);
    printf("PASS\n");
    return 0;
}
