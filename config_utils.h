#ifndef CONFIG_UTILS_H
#define CONFIG_UTILS_H

/**
 * Returns a static buffer containing the path to the configuration file or directory.
 * If filename is empty, returns the path to the config directory.
 * The buffer is overwritten on subsequent calls.
 */
char* get_config_path(const char* filename);

/**
 * Ensures that the configuration directory and cache directory exist.
 * Creates them if they do not exist.
 * Returns 0 on success, -1 on failure.
 */
int ensure_config_dir_exists();

#endif
