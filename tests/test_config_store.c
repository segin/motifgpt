#include "../config_store.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

// Helper to remove directory recursively (simple version for test)
void remove_directory(const char *path) {
    char command[1024];
    snprintf(command, sizeof(command), "rm -rf %s", path);
    system(command);
}

void test_save_and_load_settings() {
    printf("Testing save_settings and load_settings...\n");

    // Setup temporary directory for config
    char temp_dir_template[] = "/tmp/motifgpt_test_XXXXXX";
    char *temp_dir = mkdtemp(temp_dir_template);
    if (!temp_dir) {
        perror("mkdtemp");
        exit(1);
    }

    // Set environment variable
    setenv("XDG_CONFIG_HOME", temp_dir, 1);

    // Prepare expected values
    current_api_provider = DP_PROVIDER_OPENAI_COMPATIBLE;
    strcpy(current_gemini_api_key, "gemini_key_123");
    strcpy(current_gemini_model, "gemini_model_abc");
    strcpy(current_openai_api_key, "openai_key_456");
    strcpy(current_openai_model, "openai_model_def");
    strcpy(current_openai_base_url, "http://localhost:1234/v1");
    strcpy(current_anthropic_api_key, "anthropic_key_789");
    strcpy(current_anthropic_model, "anthropic_model_ghi");
    current_max_history_messages = 42;
    history_limits_disabled = true;
    enter_key_sends_message = false;
    strcpy(current_system_prompt, "You are a test bot.");
    append_default_system_prompt = false;

    // Save settings
    save_settings();

    // Verify file exists
    char config_path[1024];
    snprintf(config_path, sizeof(config_path), "%s/motifgpt/%s", temp_dir, CONFIG_FILE_NAME);
    if (access(config_path, F_OK) != 0) {
        fprintf(stderr, "Config file not created at %s\n", config_path);
        remove_directory(temp_dir);
        exit(1);
    }

    // Reset globals to verify loading
    current_api_provider = DP_PROVIDER_GOOGLE_GEMINI; // Default
    current_gemini_api_key[0] = '\0';
    current_max_history_messages = 0;
    history_limits_disabled = false;
    enter_key_sends_message = true;
    append_default_system_prompt = true; // Default is true
    current_system_prompt[0] = '\0';

    // Load settings
    load_settings();

    // Verify loaded values
    assert(current_api_provider == DP_PROVIDER_OPENAI_COMPATIBLE);
    assert(strcmp(current_gemini_api_key, "gemini_key_123") == 0);
    assert(strcmp(current_gemini_model, "gemini_model_abc") == 0);
    assert(strcmp(current_openai_api_key, "openai_key_456") == 0);
    assert(strcmp(current_openai_model, "openai_model_def") == 0);
    assert(strcmp(current_openai_base_url, "http://localhost:1234/v1") == 0);
    assert(strcmp(current_anthropic_api_key, "anthropic_key_789") == 0);
    assert(strcmp(current_anthropic_model, "anthropic_model_ghi") == 0);
    assert(current_max_history_messages == 42);
    assert(history_limits_disabled == true);
    assert(enter_key_sends_message == false);
    assert(strcmp(current_system_prompt, "You are a test bot.") == 0);

    assert(append_default_system_prompt == false);

    // Cleanup
    remove_directory(temp_dir);

    printf("Test passed!\n");
}

void test_load_settings_no_file() {
    printf("Testing load_settings without config file (env var fallback)...\n");

    // Setup temporary directory for config
    char temp_dir_template[] = "/tmp/motifgpt_test_no_file_XXXXXX";
    char *temp_dir = mkdtemp(temp_dir_template);
    if (!temp_dir) {
        perror("mkdtemp");
        exit(1);
    }

    // Set environment variable
    setenv("XDG_CONFIG_HOME", temp_dir, 1);
    setenv("GEMINI_API_KEY", "env_gemini_key", 1);
    setenv("OPENAI_API_KEY", "env_openai_key", 1);

    // Reset globals to ensure we are testing loading
    current_api_provider = DP_PROVIDER_GOOGLE_GEMINI;
    current_gemini_api_key[0] = '\0';
    current_openai_api_key[0] = '\0';
    current_max_history_messages = 0;
    history_limits_disabled = true; // Set to non-default
    enter_key_sends_message = false; // Set to non-default

    // Load settings
    load_settings();

    // Verify loaded values from environment and defaults
    assert(strcmp(current_gemini_api_key, "env_gemini_key") == 0);
    assert(strcmp(current_openai_api_key, "env_openai_key") == 0);
    assert(current_max_history_messages == DEFAULT_MAX_HISTORY_MESSAGES);
    assert(history_limits_disabled == false);
    assert(enter_key_sends_message == true);

    // Cleanup
    unsetenv("GEMINI_API_KEY");
    unsetenv("OPENAI_API_KEY");
    remove_directory(temp_dir);

    printf("Test passed!\n");
}

int main() {
    test_save_and_load_settings();
    test_load_settings_no_file();
    return 0;
}
