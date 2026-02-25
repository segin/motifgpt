#include "../config.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

// Helper to clean up
void cleanup_temp_home(const char* temp_home) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", temp_home);
    system(cmd);
}

void test_load_settings_from_file() {
    printf("Running test_load_settings_from_file...\n");

    // Create temp home
    char temp_home[] = "/tmp/motifgpt_test_XXXXXX";
    if (!mkdtemp(temp_home)) {
        perror("mkdtemp");
        exit(1);
    }

    // Set HOME
    if (setenv("HOME", temp_home, 1) != 0) {
        perror("setenv");
        exit(1);
    }
    unsetenv("XDG_CONFIG_HOME"); // Ensure we use HOME

    // Create config dir
    char config_dir[1024];
    snprintf(config_dir, sizeof(config_dir), "%s/.config/motifgpt", temp_home);
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "mkdir -p %s", config_dir);
    if (system(cmd) != 0) {
        fprintf(stderr, "Failed to create config dir\n");
        exit(1);
    }

    // Write settings.conf
    char settings_path[1024];
    snprintf(settings_path, sizeof(settings_path), "%s/settings.conf", config_dir);
    FILE *fp = fopen(settings_path, "w");
    if (!fp) {
        perror("fopen settings.conf");
        exit(1);
    }
    fprintf(fp, "provider=anthropic\n");
    fprintf(fp, "gemini_api_key=test_gemini_key\n");
    fprintf(fp, "gemini_model=test_gemini_model\n");
    fprintf(fp, "openai_api_key=test_openai_key\n");
    fprintf(fp, "openai_model=test_openai_model\n");
    fprintf(fp, "openai_base_url=http://test.url\n");
    fprintf(fp, "anthropic_api_key=test_anthropic_key\n");
    fprintf(fp, "anthropic_model=test_anthropic_model\n");
    fprintf(fp, "max_history=50\n");
    fprintf(fp, "system_prompt=You are a test bot.\n");
    fprintf(fp, "history_limits_disabled=true\n");
    fprintf(fp, "enter_sends_message=false\n");
    fclose(fp);

    // Call load_settings
    load_settings();

    // Verify
    assert(current_api_provider == DP_PROVIDER_ANTHROPIC);
    assert(strcmp(current_gemini_api_key, "test_gemini_key") == 0);
    assert(strcmp(current_gemini_model, "test_gemini_model") == 0);
    assert(strcmp(current_openai_api_key, "test_openai_key") == 0);
    assert(strcmp(current_openai_model, "test_openai_model") == 0);
    assert(strcmp(current_openai_base_url, "http://test.url") == 0);
    assert(strcmp(current_anthropic_api_key, "test_anthropic_key") == 0);
    assert(strcmp(current_anthropic_model, "test_anthropic_model") == 0);
    assert(current_max_history_messages == 50);
    assert(strcmp(current_system_prompt, "You are a test bot.") == 0);
    assert(history_limits_disabled == true);
    assert(enter_key_sends_message == false);

    printf("test_load_settings_from_file PASSED\n");
    cleanup_temp_home(temp_home);
}

void test_load_settings_defaults_and_env() {
    printf("Running test_load_settings_defaults_and_env...\n");

    // Create temp home (empty)
    char temp_home[] = "/tmp/motifgpt_test_env_XXXXXX";
    if (!mkdtemp(temp_home)) {
        perror("mkdtemp");
        exit(1);
    }
    if (setenv("HOME", temp_home, 1) != 0) {
        perror("setenv");
        exit(1);
    }
    unsetenv("XDG_CONFIG_HOME");

    // Set env vars
    setenv("GEMINI_API_KEY", "env_gemini_key", 1);
    setenv("OPENAI_API_KEY", "env_openai_key", 1);

    // Ensure globals are reset (load_settings doesn't fully reset all globals if file missing, it mainly sets keys from env)
    // Actually, load_settings implementation:
    // If !fp: sets defaults/env vars.
    // current_gemini_api_key = getenv...
    // current_openai_api_key = getenv...
    // current_max_history_messages = DEFAULT...
    // history_limits_disabled = False
    // enter_key_sends_message = True
    // It does NOT reset models or system prompt. They retain initialization values (or previous values).
    // So we should manually verify what it touches.

    // Reset globals to something else to verify they change or stay default
    current_max_history_messages = 999;

    load_settings();

    assert(strcmp(current_gemini_api_key, "env_gemini_key") == 0);
    assert(strcmp(current_openai_api_key, "env_openai_key") == 0);
    assert(current_max_history_messages == DEFAULT_MAX_HISTORY_MESSAGES);
    assert(history_limits_disabled == false);
    assert(enter_key_sends_message == true);

    printf("test_load_settings_defaults_and_env PASSED\n");
    cleanup_temp_home(temp_home);
}

int main() {
    test_load_settings_from_file();
    test_load_settings_defaults_and_env();
    printf("All tests PASSED\n");
    return 0;
}
