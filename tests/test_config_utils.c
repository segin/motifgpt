#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "config_utils.h"

int main() {
    char *result;

    // Test 1: XDG_CONFIG_HOME set
    setenv("XDG_CONFIG_HOME", "/tmp/xdg_config", 1);
    result = get_config_path("test.conf");
    assert(result != NULL);
    // get_config_path uses static buffer, so we must be careful if we called it again (but here we assert immediately)
    // Also, path construction: "%s/motifgpt/%s" -> "/tmp/xdg_config/motifgpt/test.conf"
    assert(strcmp(result, "/tmp/xdg_config/motifgpt/test.conf") == 0);
    printf("Test 1 Passed: XDG_CONFIG_HOME -> %s\n", result);

    // Test 2: XDG_CONFIG_HOME not set, HOME set
    unsetenv("XDG_CONFIG_HOME");
    setenv("HOME", "/home/user", 1);
    result = get_config_path("test.conf");
    assert(result != NULL);
    // fallback: "%s/.config/motifgpt/%s" -> "/home/user/.config/motifgpt/test.conf"
    assert(strcmp(result, "/home/user/.config/motifgpt/test.conf") == 0);
    printf("Test 2 Passed: HOME fallback -> %s\n", result);

    // Test 3: NULL filename
    // Should behave like empty string
    result = get_config_path(NULL);
    assert(result != NULL);
    assert(strcmp(result, "/home/user/.config/motifgpt/") == 0);
    printf("Test 3 Passed: NULL filename -> %s\n", result);

    // Test 4: Empty filename
    result = get_config_path("");
    assert(result != NULL);
    assert(strcmp(result, "/home/user/.config/motifgpt/") == 0);
    printf("Test 4 Passed: Empty filename -> %s\n", result);

    // Test 5: HOME not set
    unsetenv("HOME");
    // stderr output expected here
    printf("Test 5: Expecting error message about HOME env var...\n");
    result = get_config_path("test.conf");
    assert(result == NULL);
    printf("Test 5 Passed: HOME not set -> NULL\n");

    printf("All tests passed!\n");
    return 0;
}
