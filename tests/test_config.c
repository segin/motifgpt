#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "../config_utils.h"

int main() {
    // Setup temp dir
    char template[] = "/tmp/motifgpt_test_XXXXXX";
    char *temp_dir = mkdtemp(template);
    if (!temp_dir) {
        perror("mkdtemp");
        return 1;
    }
    printf("Temp dir: %s\n", temp_dir);

    // Set XDG_CONFIG_HOME
    if (setenv("XDG_CONFIG_HOME", temp_dir, 1) != 0) {
        perror("setenv");
        return 1;
    }

    // Run test
    if (ensure_config_dir_exists() != 0) {
        fprintf(stderr, "ensure_config_dir_exists failed\n");
        return 1;
    }

    // Verify directories
    char path[1024];
    struct stat st;

    // Check base dir
    snprintf(path, sizeof(path), "%s/motifgpt", temp_dir);
    if (stat(path, &st) != 0) {
        perror("stat motifgpt dir");
        return 1;
    }
    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "%s is not a directory\n", path);
        return 1;
    }

    // Check cache dir
    snprintf(path, sizeof(path), "%s/motifgpt/cache", temp_dir);
    if (stat(path, &st) != 0) {
        perror("stat cache dir");
        return 1;
    }
    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "%s is not a directory\n", path);
        return 1;
    }

    printf("Test passed!\n");

    // Cleanup
    snprintf(path, sizeof(path), "rm -rf %s", temp_dir);
    system(path);

    return 0;
}
