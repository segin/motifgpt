#ifndef CONFIG_UTILS_H
#define CONFIG_UTILS_H

#include <limits.h>

#define CONFIG_DIR_MODE 0755
#define CACHE_DIR_NAME "cache"

char* get_config_path(const char* filename);
int ensure_config_dir_exists();

#endif
