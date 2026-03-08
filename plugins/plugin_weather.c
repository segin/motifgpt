#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "motifgpt_plugin.h"
#include <cjson/cJSON.h>

char* weather_execute(const char* args_json) {
    cJSON* json = cJSON_Parse(args_json);
    if (!json) return strdup("Error: Invalid JSON arguments");

    cJSON* location_param = cJSON_GetObjectItemCaseSensitive(json, "location");
    if (!cJSON_IsString(location_param) || (location_param->valuestring == NULL)) {
        cJSON_Delete(json);
        return strdup("Error: Missing 'location' string parameter");
    }

    // This is a mock implementation
    char result_buf[256];
    snprintf(result_buf, sizeof(result_buf), "The weather in %s is currently 72°F and sunny.", location_param->valuestring);

    cJSON_Delete(json);
    return strdup(result_buf);
}

static motifgpt_tool_t tools[] = {
    {
        "get_weather",
        "Get the current weather for a specified location.",
        "{\"type\": \"object\", \"properties\": {\"location\": {\"type\": \"string\", \"description\": \"The city and state, e.g., San Francisco, CA\"}}, \"required\": [\"location\"]}",
        weather_execute
    }
};

static motifgpt_plugin_t plugin = {
    "WeatherPlugin",
    tools,
    1
};

motifgpt_plugin_t* motifgpt_plugin_init(void) {
    return &plugin;
}
