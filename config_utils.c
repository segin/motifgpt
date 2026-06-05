#include "config_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <wordexp.h>
#include <errno.h>

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
        if (wordexp(path, &p, WRDE_NOCMD) == 0) {
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
    if (strlen(base_dir_path) + 1 + strlen(CACHE_DIR_NAME) >= sizeof(cache_dir_full_path)) {
        fprintf(stderr, "Path too long for cache dir.\n");
        return -1;
    }
    snprintf(cache_dir_full_path, sizeof(cache_dir_full_path), "%s/%s", base_dir_path, CACHE_DIR_NAME);
    if (stat(cache_dir_full_path, &st) == -1) {
        if (mkdir(cache_dir_full_path, CONFIG_DIR_MODE) == -1 && errno != EEXIST) {
            char err_msg[PATH_MAX + 100]; snprintf(err_msg, sizeof(err_msg), "mkdir cache dir: %s", cache_dir_full_path);
            perror(err_msg);
        } else { printf("Created cache directory: %s\n", cache_dir_full_path); }
    }
    return 0;
}
