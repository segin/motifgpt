#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include "config_utils.h"

int dir_exists(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    return 0;
}

int main() {
    char template[] = "/tmp/motifgpt_test_XXXXXX";
    char *temp_dir = mkdtemp(template);

    if (!temp_dir) {
        perror("mkdtemp");
        return 1;
    }

    printf("Temporary test directory: %s\n", temp_dir);

    if (setenv("HOME", temp_dir, 1) != 0) {
        perror("setenv HOME");
        return 1;
    }

    unsetenv("XDG_CONFIG_HOME");

    if (ensure_config_dir_exists() != 0) {
        fprintf(stderr, "ensure_config_dir_exists failed\n");
        return 1;
    }

    char config_path[1024];
    char cache_path[1024];

    snprintf(config_path, sizeof(config_path), "%s/.config/motifgpt", temp_dir);
    snprintf(cache_path, sizeof(cache_path), "%s/.config/motifgpt/cache", temp_dir);

    if (!dir_exists(config_path)) {
        fprintf(stderr, "FAIL: Config dir not created: %s\n", config_path);
        return 1;
    } else {
        printf("PASS: Config dir created: %s\n", config_path);
    }

    if (!dir_exists(cache_path)) {
        fprintf(stderr, "FAIL: Cache dir not created: %s\n", cache_path);
        return 1;
    } else {
        printf("PASS: Cache dir created: %s\n", cache_path);
    }

    // Test 2: XDG_CONFIG_HOME
    char xdg_dir[1024];
    snprintf(xdg_dir, sizeof(xdg_dir), "%s/myconfig", temp_dir);
    if (mkdir(xdg_dir, 0755) != 0) {
        perror("mkdir xdg_dir");
        return 1;
    }
    setenv("XDG_CONFIG_HOME", xdg_dir, 1);

    if (ensure_config_dir_exists() != 0) {
        fprintf(stderr, "ensure_config_dir_exists (XDG) failed\n");
        return 1;
    }

    char xdg_motifgpt_path[1024];
    snprintf(xdg_motifgpt_path, sizeof(xdg_motifgpt_path), "%s/motifgpt", xdg_dir);

    if (!dir_exists(xdg_motifgpt_path)) {
        fprintf(stderr, "FAIL: XDG Config dir not created: %s\n", xdg_motifgpt_path);
        return 1;
    } else {
        printf("PASS: XDG Config dir created: %s\n", xdg_motifgpt_path);
    }

    // Cleanup
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", temp_dir);
    if (system(cmd) != 0) {
        fprintf(stderr, "Warning: Failed to cleanup temp dir %s\n", temp_dir);
    }

    return 0;
}
