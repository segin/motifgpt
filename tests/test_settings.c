#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>

#include "../motifgpt.c"

// Stubs for libdisasterparty
dp_context_t* dp_init_context(dp_provider_type_t provider, const char *api_key, const char *base_url) { return (dp_context_t*)1; }
void dp_destroy_context(dp_context_t *ctx) {}
int dp_perform_streaming_completion(dp_context_t *ctx, dp_request_config_t *config, dp_stream_callback_t callback, void *user_data, dp_response_t *response) { return 0; }
void dp_free_response_content(dp_response_t *response) {}
int dp_message_add_text_part(dp_message_t *msg, const char *text) { return 1; }
int dp_message_add_base64_image_part(dp_message_t *msg, const char *mime_type, const char *base64_data) { return 1; }
void dp_free_messages(dp_message_t *msgs, size_t count) {}
int dp_list_models(dp_context_t *ctx, dp_model_list_t **list) { return 0; }
void dp_free_model_list(dp_model_list_t *list) {}
int dp_serialize_messages_to_file(dp_message_t *msgs, size_t count, const char *filename) { return 0; }
int dp_deserialize_messages_from_file(const char *filename, dp_message_t **msgs, size_t *count) { return 0; }

// Helper to create a temp directory
char *create_temp_config_dir() {
    char *template = strdup("/tmp/motifgpt_test_XXXXXX");
    if (!mkdtemp(template)) {
        perror("mkdtemp");
        free(template);
        return NULL;
    }
    return template;
}

// Helper to clean up temp directory
void cleanup_temp_config_dir(const char *path) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", path);
    if (system(cmd) != 0) {
        perror("system");
    }
}

START_TEST(test_load_settings_file)
{
    // Setup
    char *tmp_dir = create_temp_config_dir();
    ck_assert_ptr_nonnull(tmp_dir);
    setenv("XDG_CONFIG_HOME", tmp_dir, 1);

    // Create motifgpt subdirectory
    char motifgpt_dir[1024];
    snprintf(motifgpt_dir, sizeof(motifgpt_dir), "%s/motifgpt", tmp_dir);
    mkdir(motifgpt_dir, 0755);

    // Create settings.conf
    char settings_file[1024];
    snprintf(settings_file, sizeof(settings_file), "%s/settings.conf", motifgpt_dir);
    FILE *fp = fopen(settings_file, "w");
    ck_assert_ptr_nonnull(fp);
    fprintf(fp, "gemini_api_key=test_gemini_key\n");
    fprintf(fp, "openai_api_key=test_openai_key\n");
    fprintf(fp, "max_history=50\n");
    fprintf(fp, "history_limits_disabled=true\n");
    fprintf(fp, "enter_sends_message=false\n");
    fclose(fp);

    // Act
    load_settings();

    // Assert
    ck_assert_str_eq(current_gemini_api_key, "test_gemini_key");
    ck_assert_str_eq(current_openai_api_key, "test_openai_key");
    ck_assert_int_eq(current_max_history_messages, 50);
    ck_assert_int_eq(history_limits_disabled, True); // True is 1 in Motif/X11 usually
    ck_assert_int_eq(enter_key_sends_message, False);

    // Cleanup
    cleanup_temp_config_dir(tmp_dir);
    free(tmp_dir);
}
END_TEST

START_TEST(test_load_settings_env_fallback)
{
    // Setup
    char *tmp_dir = create_temp_config_dir();
    ck_assert_ptr_nonnull(tmp_dir);
    setenv("XDG_CONFIG_HOME", tmp_dir, 1);

    // Ensure no settings file exists (dir is empty/new)
    // Create base dir so it doesn't fail on mkdir if code checks
    char motifgpt_dir[512];
    snprintf(motifgpt_dir, sizeof(motifgpt_dir), "%s/motifgpt", tmp_dir);
    mkdir(motifgpt_dir, 0755);

    setenv("GEMINI_API_KEY", "env_gemini_key", 1);
    setenv("OPENAI_API_KEY", "env_openai_key", 1);

    // Reset globals to ensure we are testing loading
    current_gemini_api_key[0] = '\0';
    current_openai_api_key[0] = '\0';
    current_max_history_messages = 0;
    history_limits_disabled = True; // Set to non-default to check reset

    // Act
    load_settings();

    // Assert
    ck_assert_str_eq(current_gemini_api_key, "env_gemini_key");
    ck_assert_str_eq(current_openai_api_key, "env_openai_key");
    ck_assert_int_eq(current_max_history_messages, DEFAULT_MAX_HISTORY_MESSAGES);
    ck_assert_int_eq(history_limits_disabled, False);
    ck_assert_int_eq(enter_key_sends_message, True);

    // Cleanup
    cleanup_temp_config_dir(tmp_dir);
    free(tmp_dir);
    unsetenv("GEMINI_API_KEY");
    unsetenv("OPENAI_API_KEY");
}
END_TEST

Suite *settings_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Settings");

    /* Core test case */
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_load_settings_file);
    tcase_add_test(tc_core, test_load_settings_env_fallback);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = settings_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
