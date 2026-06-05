#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "motifgpt_plugin.h"

#define MAX_TOOLS 64
motifgpt_tool_t* registry_tools[MAX_TOOLS];
int num_registry_tools = 0;

void original_append_tools_to_system_prompt(char* buffer, size_t buffer_size) {
    if (num_registry_tools == 0) return;

    const char* header = "\n\nYou have access to the following tools. To call a tool, you MUST output a JSON block inside a <tool_call> tag. Wait for the user to provide the tool result in a <tool_result> tag. DO NOT output anything else when calling a tool.\nFormat:\n<tool_call>{\"name\": \"tool_name\", \"args\": {\"arg1\": \"val1\"}}</tool_call>\n\nAvailable tools:\n";
    strncat(buffer, header, buffer_size - strlen(buffer) - 1);

    for (int i = 0; i < num_registry_tools; i++) {
        char tool_desc[2048];
        snprintf(tool_desc, sizeof(tool_desc), "- %s: %s\n  Parameters: %s\n", registry_tools[i]->name, registry_tools[i]->description, registry_tools[i]->parameters_schema);
        strncat(buffer, tool_desc, buffer_size - strlen(buffer) - 1);
    }
}

void optimized_append_tools_to_system_prompt(char* buffer, size_t buffer_size) {
    if (num_registry_tools == 0) return;

    size_t current_len = strlen(buffer);
    const char* header = "\n\nYou have access to the following tools. To call a tool, you MUST output a JSON block inside a <tool_call> tag. Wait for the user to provide the tool result in a <tool_result> tag. DO NOT output anything else when calling a tool.\nFormat:\n<tool_call>{\"name\": \"tool_name\", \"args\": {\"arg1\": \"val1\"}}</tool_call>\n\nAvailable tools:\n";

    size_t remaining = buffer_size - current_len;
    if (remaining > 1) {
        int written = snprintf(buffer + current_len, remaining, "%s", header);
        if (written > 0) {
            if ((size_t)written >= remaining) {
                current_len += remaining - 1;
            } else {
                current_len += written;
            }
        }
    }

    for (int i = 0; i < num_registry_tools; i++) {
        char tool_desc[2048];
        int desc_len = snprintf(tool_desc, sizeof(tool_desc), "- %s: %s\n  Parameters: %s\n", registry_tools[i]->name, registry_tools[i]->description, registry_tools[i]->parameters_schema);

        remaining = buffer_size - current_len;
        if (remaining > 1) {
            int written = snprintf(buffer + current_len, remaining, "%s", tool_desc);
            if (written > 0) {
                if ((size_t)written >= remaining) {
                    current_len += remaining - 1;
                } else {
                    current_len += written;
                }
            }
        }
    }
}

int main() {
    num_registry_tools = MAX_TOOLS;
    for (int i = 0; i < MAX_TOOLS; i++) {
        registry_tools[i] = malloc(sizeof(motifgpt_tool_t));
        registry_tools[i]->name = "test_tool";
        registry_tools[i]->description = "This is a test tool description for benchmarking.";
        registry_tools[i]->parameters_schema = "{\"type\": \"object\", \"properties\": {\"arg1\": {\"type\": \"string\"}}}";
    }

    char buffer1[16384];
    char buffer2[16384];

    const int iterations = 10000;

    clock_t start = clock();
    for (int i = 0; i < iterations; i++) {
        buffer1[0] = '\0';
        original_append_tools_to_system_prompt(buffer1, sizeof(buffer1));
    }
    clock_t end = clock();
    double time_orig = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Original: %f s\n", time_orig);

    start = clock();
    for (int i = 0; i < iterations; i++) {
        buffer2[0] = '\0';
        optimized_append_tools_to_system_prompt(buffer2, sizeof(buffer2));
    }
    end = clock();
    double time_opt = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Optimized: %f s\n", time_opt);
    printf("Speedup: %.2fx\n", time_orig / time_opt);

    if (strcmp(buffer1, buffer2) == 0) {
        printf("Verification: SUCCESS\n");
    } else {
        printf("Verification: FAILURE\n");
        // printf("Orig:\n%s\n", buffer1);
        // printf("Opt:\n%s\n", buffer2);
    }

    for (int i = 0; i < MAX_TOOLS; i++) {
        free(registry_tools[i]);
    }

    return 0;
}
