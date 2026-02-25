#ifndef DISASTERPARTY_H
#define DISASTERPARTY_H

#include <stddef.h>
#include <stdbool.h>

typedef enum {
    DP_PROVIDER_GOOGLE_GEMINI,
    DP_PROVIDER_OPENAI_COMPATIBLE,
    DP_PROVIDER_ANTHROPIC
} dp_provider_type_t;

typedef enum {
    DP_ROLE_USER,
    DP_ROLE_ASSISTANT
} dp_message_role_t;

typedef enum {
    DP_CONTENT_PART_TEXT,
    DP_CONTENT_PART_IMAGE_BASE64
} dp_content_part_type_t;

typedef struct dp_content_part {
    dp_content_part_type_t type;
    char *text;
    char *image_mime_type;
    char *image_base64_data;
} dp_content_part_t;

typedef struct dp_message {
    dp_message_role_t role;
    dp_content_part_t *parts;
    size_t num_parts;
} dp_message_t;

typedef struct dp_context dp_context_t;

typedef struct dp_request_config {
    const char *model;
    const char *system_prompt;
    double temperature;
    int max_tokens;
    bool stream;
    dp_message_t *messages;
    size_t num_messages;
} dp_request_config_t;

typedef struct dp_response {
    char *error_message;
    long http_status_code;
} dp_response_t;

typedef int (*dp_stream_callback_t)(const char *token, void *user_data, bool is_final, const char *error_message);

typedef struct dp_model_info {
    char *model_id;
} dp_model_info_t;

typedef struct dp_model_list {
    dp_model_info_t *models;
    size_t count;
    char *error_message;
    long http_status_code;
} dp_model_list_t;

// Function prototypes
dp_context_t* dp_init_context(dp_provider_type_t provider, const char* api_key, const char* base_url);
void dp_destroy_context(dp_context_t* ctx);
int dp_perform_streaming_completion(dp_context_t* ctx, dp_request_config_t* config, dp_stream_callback_t callback, void* user_data, dp_response_t* response);
int dp_list_models(dp_context_t* ctx, dp_model_list_t** model_list);
void dp_free_model_list(dp_model_list_t* list);
void dp_free_response_content(dp_response_t* response);
int dp_message_add_text_part(dp_message_t* msg, const char* text);
int dp_message_add_base64_image_part(dp_message_t* msg, const char* mime_type, const char* base64_data);
void dp_free_messages(dp_message_t* messages, size_t count);
int dp_serialize_messages_to_file(dp_message_t* messages, size_t count, const char* filename);
int dp_deserialize_messages_from_file(const char* filename, dp_message_t** messages, size_t* count);

#endif
