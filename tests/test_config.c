#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include "../motifgpt_config.h"

int main() {
    printf("Starting get_config_path tests...\n");

    // Test 1: XDG_CONFIG_HOME set
    printf("Test 1: XDG_CONFIG_HOME set\n");
    unsetenv("HOME");
    setenv("XDG_CONFIG_HOME", "/tmp/config", 1);
    char *path = get_config_path("settings.conf");
    assert(path != NULL);
    printf("Expected: /tmp/config/motifgpt/settings.conf, Got: %s\n", path);
    assert(strcmp(path, "/tmp/config/motifgpt/settings.conf") == 0);

    // Test 2: XDG_CONFIG_HOME unset, HOME set
    printf("Test 2: XDG_CONFIG_HOME unset, HOME set\n");
    unsetenv("XDG_CONFIG_HOME");
    setenv("HOME", "/tmp/home", 1);
    path = get_config_path("settings.conf");
    assert(path != NULL);
    printf("Expected: /tmp/home/.config/motifgpt/settings.conf, Got: %s\n", path);
    assert(strcmp(path, "/tmp/home/.config/motifgpt/settings.conf") == 0);

    // Test 3: HOME unset
    printf("Test 3: HOME unset\n");
    unsetenv("HOME");
    unsetenv("XDG_CONFIG_HOME");
    path = get_config_path("settings.conf");
    if (path == NULL) {
        printf("Got NULL as expected.\n");
    } else {
        printf("Error: Expected NULL, Got: %s\n", path);
        return 1;
    }

    // Test 4: Empty filename
    printf("Test 4: Empty filename\n");
    setenv("HOME", "/tmp/home", 1);
    path = get_config_path(""); // Should return base dir path
    assert(path != NULL);
    printf("Expected: /tmp/home/.config/motifgpt/, Got: %s\n", path);
    assert(strcmp(path, "/tmp/home/.config/motifgpt/") == 0);

    // Test 5: NULL filename
    printf("Test 5: NULL filename\n");
    path = get_config_path(NULL);
    assert(path != NULL);
    printf("Expected: /tmp/home/.config/motifgpt/, Got: %s\n", path);
    assert(strcmp(path, "/tmp/home/.config/motifgpt/") == 0);

    printf("All tests passed!\n");
    return 0;
}
