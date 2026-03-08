#ifndef MOTIFGPT_PLUGIN_H
#define MOTIFGPT_PLUGIN_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* name;
    const char* description;
    const char* parameters_schema; // JSON schema string
    char* (*execute)(const char* args_json); // Returns allocated string with result, must be free()able
} motifgpt_tool_t;

typedef struct {
    const char* plugin_name;
    motifgpt_tool_t* tools;
    int num_tools;
} motifgpt_plugin_t;

// Plugins must export this function
typedef motifgpt_plugin_t* (*motifgpt_plugin_init_func)(void);

#ifdef __cplusplus
}
#endif

#endif // MOTIFGPT_PLUGIN_H
