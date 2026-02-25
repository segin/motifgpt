#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H

#include <stdbool.h>
#include "disasterparty.h"

// --- Configuration ---
#define DEFAULT_PROVIDER DP_PROVIDER_GOOGLE_GEMINI
#define DEFAULT_GEMINI_MODEL "gemini-2.0-flash"
#define DEFAULT_OPENAI_MODEL "gpt-4.1-nano"
#define DEFAULT_OPENAI_BASE_URL "https://api.openai.com/v1"
#define DEFAULT_ANTHROPIC_MODEL "claude-3-haiku-20240307"
#define DEFAULT_GEMINI_KEY_PLACEHOLDER "AIkeygoesherexxx..."
#define DEFAULT_OPENAI_KEY_PLACEHOLDER "sk-yourkeygoesherexxxx..."
#define DEFAULT_ANTHROPIC_KEY_PLACEHOLDER "sk-ant-yourkeygoesherexxxx..."
#define USER_NICKNAME "User"
#define ASSISTANT_NICKNAME "Assistant"
#define DEFAULT_MAX_HISTORY_MESSAGES 100
#define INTERNAL_MAX_HISTORY_CAPACITY 10000
#define CONFIG_DIR_MODE 0755
#define CONFIG_FILE_NAME "settings.conf"
#define CACHE_DIR_NAME "cache"
// --- End Configuration ---

extern dp_provider_type_t current_api_provider;
extern char current_gemini_api_key[256];
extern char current_gemini_model[128];
extern char current_openai_api_key[256];
extern char current_openai_model[128];
extern char current_openai_base_url[256];
extern char current_anthropic_api_key[256];
extern char current_anthropic_model[128];
extern int current_max_history_messages;
extern bool history_limits_disabled;
extern bool enter_key_sends_message;
extern char current_system_prompt[2048];
extern bool append_default_system_prompt;

void load_settings();
void save_settings();
char* get_config_path(const char* filename);
int ensure_config_dir_exists();

#endif
