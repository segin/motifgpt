#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <wordexp.h>
#include <limits.h>
#include <ctype.h>

// --- Global Variables ---
dp_provider_type_t current_api_provider = DEFAULT_PROVIDER;
char current_gemini_api_key[API_KEY_BUF_SIZE] = "";
char current_gemini_model[MODEL_ID_BUF_SIZE] = DEFAULT_GEMINI_MODEL;
char current_openai_api_key[API_KEY_BUF_SIZE] = "";
char current_openai_model[MODEL_ID_BUF_SIZE] = DEFAULT_OPENAI_MODEL;
char current_openai_base_url[URL_BUF_SIZE] = "";
char current_anthropic_api_key[API_KEY_BUF_SIZE] = "";
char current_anthropic_model[MODEL_ID_BUF_SIZE] = DEFAULT_ANTHROPIC_MODEL;
int current_max_history_messages = DEFAULT_MAX_HISTORY_MESSAGES;
bool history_limits_disabled = false;
bool enter_key_sends_message = true;
char current_system_prompt[SYSTEM_PROMPT_BUF_SIZE] = "";
bool append_default_system_prompt = true;

// --- Implementations ---

char* get_config_path(const char* filename) {
    static char path[PATH_MAX]; wordexp_t p; char *env_path;
    if (filename == NULL) filename = "";
    env_path = getenv("XDG_CONFIG_HOME");
    if (env_path && env_path[0]) {
        snprintf(path, sizeof(path), "%s/motifgpt/%s", env_path, filename);
    } else {
        env_path = getenv("HOME");
        if (!env_path || !env_path[0]) {
            fprintf(stderr, "Error: HOME env var not set.\n"); return NULL;
        }
        snprintf(path, sizeof(path), "%s/.config/motifgpt/%s", env_path, filename);
    }
    if (path[0] == '~') {
        if (wordexp(path, &p, 0) == 0) {
            if (p.we_wordc > 0) strncpy(path, p.we_wordv[0], sizeof(path) - 1);
            path[sizeof(path) - 1] = '\0'; wordfree(&p);
        } else { fprintf(stderr, "wordexp failed for path: %s\n", path); }
    }
    return path;
}

int ensure_config_dir_exists() {
    char *base_dir_path_ptr = get_config_path(""); if (!base_dir_path_ptr) return -1;
    char base_dir_path[PATH_MAX]; strncpy(base_dir_path, base_dir_path_ptr, PATH_MAX -1); base_dir_path[PATH_MAX-1] = '\0';
    if (base_dir_path[strlen(base_dir_path)-1] == '/') base_dir_path[strlen(base_dir_path)-1] = '\0';
    struct stat st = {0};
    if (stat(base_dir_path, &st) == -1) {
        if (mkdir(base_dir_path, CONFIG_DIR_MODE) == -1 && errno != EEXIST) {
            char err_msg[PATH_MAX + 100]; snprintf(err_msg, sizeof(err_msg), "mkdir base config dir: %s", base_dir_path);
            perror(err_msg); return -1;
        }
        printf("Created config directory: %s\n", base_dir_path);
    }
    char cache_dir_full_path[PATH_MAX];
    snprintf(cache_dir_full_path, sizeof(cache_dir_full_path), "%s/%s", base_dir_path, CACHE_DIR_NAME);
    if (stat(cache_dir_full_path, &st) == -1) {
        if (mkdir(cache_dir_full_path, CONFIG_DIR_MODE) == -1 && errno != EEXIST) {
            char err_msg[PATH_MAX + 100]; snprintf(err_msg, sizeof(err_msg), "mkdir cache dir: %s", cache_dir_full_path);
            perror(err_msg);
        } else { printf("Created cache directory: %s\n", cache_dir_full_path); }
    }
    return 0;
}

void load_settings() {
    char *settings_file = get_config_path(CONFIG_FILE_NAME);
    if (!settings_file) { fprintf(stderr, "Could not determine settings file path.\n"); return; }
    FILE *fp = fopen(settings_file, "r");
    if (!fp) {
        printf("No settings file (%s). Using defaults/environment variables.\n", settings_file);
        const char* ge = getenv("GEMINI_API_KEY"); if (ge) strncpy(current_gemini_api_key, ge, sizeof(current_gemini_api_key)-1); else current_gemini_api_key[0] = '\0';
        const char* oe = getenv("OPENAI_API_KEY"); if (oe) strncpy(current_openai_api_key, oe, sizeof(current_openai_api_key)-1); else current_openai_api_key[0] = '\0';
        current_max_history_messages = DEFAULT_MAX_HISTORY_MESSAGES;
        history_limits_disabled = false;
        enter_key_sends_message = true;
        return;
    }
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        char *key = strtok(line, "="); char *value = strtok(NULL, "\n");
        if (key && value) {
            if (strcmp(key, "provider") == 0) {
                if (strcmp(value, "gemini") == 0) current_api_provider = DP_PROVIDER_GOOGLE_GEMINI;
                else if (strcmp(value, "openai") == 0) current_api_provider = DP_PROVIDER_OPENAI_COMPATIBLE;
                else if (strcmp(value, "anthropic") == 0) current_api_provider = DP_PROVIDER_ANTHROPIC;
            } else if (strcmp(key, "gemini_api_key") == 0) strncpy(current_gemini_api_key, value, sizeof(current_gemini_api_key)-1);
            else if (strcmp(key, "gemini_model") == 0) strncpy(current_gemini_model, value, sizeof(current_gemini_model)-1);
            else if (strcmp(key, "openai_api_key") == 0) strncpy(current_openai_api_key, value, sizeof(current_openai_api_key)-1);
            else if (strcmp(key, "openai_model") == 0) strncpy(current_openai_model, value, sizeof(current_openai_model)-1);
            else if (strcmp(key, "openai_base_url") == 0) strncpy(current_openai_base_url, value, sizeof(current_openai_base_url)-1);
            else if (strcmp(key, "anthropic_api_key") == 0) strncpy(current_anthropic_api_key, value, sizeof(current_anthropic_api_key)-1);
            else if (strcmp(key, "anthropic_model") == 0) strncpy(current_anthropic_model, value, sizeof(current_anthropic_model)-1);
            else if (strcmp(key, "max_history") == 0) current_max_history_messages = atoi(value);
            else if (strcmp(key, "system_prompt") == 0) strncpy(current_system_prompt, value, sizeof(current_system_prompt)-1);
            else if (strcmp(key, "history_limits_disabled") == 0) history_limits_disabled = (strcmp(value, "true") == 0);
            else if (strcmp(key, "enter_sends_message") == 0) enter_key_sends_message = (strcmp(value, "true") == 0);

        }
    }
    fclose(fp); printf("Settings loaded from %s\n", settings_file);
}

void save_settings() {
    if (ensure_config_dir_exists() != 0) { fprintf(stderr, "Config dir error. Settings not saved.\n"); return; }
    char *settings_file = get_config_path(CONFIG_FILE_NAME);
    if (!settings_file) { fprintf(stderr, "Settings file path error. Not saved.\n"); return; }
    FILE *fp = fopen(settings_file, "w");
    if (!fp) {
        char err_msg[PATH_MAX + 100]; snprintf(err_msg, sizeof(err_msg), "fopen for writing: %s", settings_file);
        perror(err_msg); return;
    }
    const char* provider_str = "gemini";
    if (current_api_provider == DP_PROVIDER_OPENAI_COMPATIBLE) provider_str = "openai";
    else if (current_api_provider == DP_PROVIDER_ANTHROPIC) provider_str = "anthropic";
    fprintf(fp, "provider=%s\n", provider_str);

    fprintf(fp, "gemini_api_key=%s\n", current_gemini_api_key);
    fprintf(fp, "gemini_model=%s\n", current_gemini_model);
    fprintf(fp, "openai_api_key=%s\n", current_openai_api_key);
    fprintf(fp, "openai_model=%s\n", current_openai_model);
    fprintf(fp, "openai_base_url=%s\n", current_openai_base_url);
    fprintf(fp, "anthropic_api_key=%s\n", current_anthropic_api_key);
    fprintf(fp, "anthropic_model=%s\n", current_anthropic_model);

    fprintf(fp, "max_history=%d\n", current_max_history_messages);
    fprintf(fp, "system_prompt=%s\n", current_system_prompt);
    fprintf(fp, "append_default_system_prompt=%s\n", append_default_system_prompt ? "true" : "false");
    fprintf(fp, "history_limits_disabled=%s\n", history_limits_disabled ? "true" : "false");
    fprintf(fp, "enter_sends_message=%s\n", enter_key_sends_message ? "true" : "false");
    fclose(fp); printf("Settings saved to %s\n", settings_file);
}
