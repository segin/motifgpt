#include "motifgpt_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <wordexp.h>

char* get_config_path(const char* filename) {
    static char path[PATH_MAX]; wordexp_t p; char *env_path;
    if (filename == NULL) filename = "";
    env_path = getenv("XDG_CONFIG_HOME");
    if (env_path && env_path[0]) {
        snprintf(path, sizeof(path), "%s/motifgpt/%s", env_path, filename);
    } else {
        env_path = getenv("HOME");
        if (!env_path || !env_path[0]) {
            fprintf(stderr, "Error: HOME env var not set.
"); return NULL;
        }
        snprintf(path, sizeof(path), "%s/.config/motifgpt/%s", env_path, filename);
    }
    if (path[0] == '~') {
        if (wordexp(path, &p, WRDE_NOCMD) == 0) {
            if (p.we_wordc > 0) strncpy(path, p.we_wordv[0], sizeof(path) - 1);
            path[sizeof(path) - 1] = '\0'; wordfree(&p);
        } else { fprintf(stderr, "wordexp failed for path: %s
", path); }
    }
    return path;
}
