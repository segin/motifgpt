#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <assert.h>
#include "disasterparty.h"
#include <X11/Intrinsic.h>

// Globals from motifgpt.c
extern dp_provider_type_t current_api_provider;
extern char current_gemini_api_key[256];
extern char current_gemini_model[128];
extern char current_openai_api_key[256];
extern char current_openai_model[128];
extern char current_openai_base_url[256];
extern char current_anthropic_api_key[256];
extern char current_anthropic_model[128];
extern int current_max_history_messages;
extern char current_system_prompt[2048];
extern Boolean append_default_system_prompt;
extern Boolean history_limits_disabled;
extern Boolean enter_key_sends_message;

extern void save_settings();

int main() {
    // 1. Setup temp dir
    char template[] = "/tmp/motifgpt_test_XXXXXX";
    char *temp_dir = mkdtemp(template);
    if (!temp_dir) {
        perror("mkdtemp");
        return 1;
    }
    printf("Temp dir: %s\n", temp_dir);

    // 2. Set XDG_CONFIG_HOME
    setenv("XDG_CONFIG_HOME", temp_dir, 1);

    // 3. Initialize globals
    current_api_provider = DP_PROVIDER_OPENAI_COMPATIBLE;
    strcpy(current_gemini_api_key, "test_gemini_key");
    strcpy(current_gemini_model, "test_gemini_model");
    strcpy(current_openai_api_key, "test_openai_key");
    strcpy(current_openai_model, "test_openai_model");
    strcpy(current_openai_base_url, "http://test.url");
    strcpy(current_anthropic_api_key, "test_anthropic_key");
    strcpy(current_anthropic_model, "test_anthropic_model");
    current_max_history_messages = 42;
    strcpy(current_system_prompt, "You are a test.");
    append_default_system_prompt = False;
    history_limits_disabled = True;
    enter_key_sends_message = False;

    // 4. Call save_settings
    save_settings();

    // 5. Verify file
    char expected_path[PATH_MAX];
    snprintf(expected_path, sizeof(expected_path), "%s/motifgpt/settings.conf", temp_dir);

    FILE *fp = fopen(expected_path, "r");
    if (!fp) {
        fprintf(stderr, "Failed to open settings file at %s\n", expected_path);
        // Clean up
        char cmd[PATH_MAX + 100];
        snprintf(cmd, sizeof(cmd), "rm -rf %s", temp_dir);
        system(cmd);
        return 1;
    }

    char buffer[4096];
    size_t len = fread(buffer, 1, sizeof(buffer)-1, fp);
    buffer[len] = '\0';
    fclose(fp);

    printf("Read settings content:\n%s\n", buffer);

    // Assertions
    int fail = 0;
    if (!strstr(buffer, "provider=openai")) { fprintf(stderr, "FAIL: provider\n"); fail = 1; }
    if (!strstr(buffer, "openai_api_key=test_openai_key")) { fprintf(stderr, "FAIL: openai_api_key\n"); fail = 1; }
    if (!strstr(buffer, "max_history=42")) { fprintf(stderr, "FAIL: max_history\n"); fail = 1; }
    if (!strstr(buffer, "history_limits_disabled=true")) { fprintf(stderr, "FAIL: history_limits_disabled\n"); fail = 1; }
    if (!strstr(buffer, "enter_sends_message=false")) { fprintf(stderr, "FAIL: enter_sends_message\n"); fail = 1; }
    if (!strstr(buffer, "system_prompt=You are a test.")) { fprintf(stderr, "FAIL: system_prompt\n"); fail = 1; }

    if (!fail) printf("Test passed!\n");

    // 6. Cleanup
    char cmd[PATH_MAX + 100];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", temp_dir);
    system(cmd);

    return fail;
}
