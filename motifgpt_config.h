#ifndef MOTIFGPT_CONFIG_H
#define MOTIFGPT_CONFIG_H

#include <limits.h>

#define CONFIG_DIR_MODE 0755
#define CACHE_DIR_NAME "cache"

/**
 * Returns the full path to a configuration file.
 * @param filename Name of the file.
 * @return Pointer to a static buffer containing the path, or NULL on error.
 */
char* get_config_path(const char* filename);

/**
 * Ensures that the configuration and cache directories exist.
 * @return 0 on success, -1 on failure.
 */
int ensure_config_dir_exists();

#endif /* MOTIFGPT_CONFIG_H */
