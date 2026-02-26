#ifndef MOTIFGPT_CONFIG_H
#define MOTIFGPT_CONFIG_H

/**
 * Returns the full path to a configuration file.
 * @param filename Name of the file.
 * @return Pointer to a static buffer containing the path, or NULL on error.
 */
char* get_config_path(const char* filename);

#endif /* MOTIFGPT_CONFIG_H */
