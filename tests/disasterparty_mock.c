#include "disasterparty.h"

dp_context_t* dp_init_context(dp_provider_type_t provider, const char *api_key, const char *base_url) { return (dp_context_t*)1; }
void dp_destroy_context(dp_context_t *ctx) {}
int dp_perform_streaming_completion(dp_context_t *ctx, dp_request_config_t *config, dp_stream_callback_t callback, void *user_data, dp_response_t *response) { return 0; }
void dp_free_response_content(dp_response_t *response) {}
int dp_message_add_text_part(dp_message_t *msg, const char *text) { return 1; }
int dp_message_add_base64_image_part(dp_message_t *msg, const char *mime_type, const char *base64_data) { return 1; }
void dp_free_messages(dp_message_t *msgs, size_t count) {}
int dp_list_models(dp_context_t *ctx, dp_model_list_t **list) { return 0; }
void dp_free_model_list(dp_model_list_t *list) {}
int dp_serialize_messages_to_fd(dp_message_t *msgs, size_t count, int fd) { return 0; }
int dp_serialize_messages_to_file(dp_message_t *msgs, size_t count, const char *filename) { return 0; }
int dp_deserialize_messages_from_file(const char *filename, dp_message_t **msgs, size_t *count) { return 0; }
