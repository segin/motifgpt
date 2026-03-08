#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "motifgpt_plugin.h"
#include <cjson/cJSON.h>

char* filereader_execute(const char* args_json) {
    cJSON* json = cJSON_Parse(args_json);
    if (!json) return strdup("Error: Invalid JSON arguments");

    cJSON* file_param = cJSON_GetObjectItemCaseSensitive(json, "filename");
    if (!cJSON_IsString(file_param) || (file_param->valuestring == NULL)) {
        cJSON_Delete(json);
        return strdup("Error: Missing 'filename' string parameter");
    }

    FILE* f = fopen(file_param->valuestring, "r");
    if (!f) {
        cJSON_Delete(json);
        return strdup("Error: Could not open file");
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* string = malloc(fsize + 1);
    if (!string) {
        fclose(f);
        cJSON_Delete(json);
        return strdup("Error: Memory allocation failed");
    }
    size_t read_bytes = fread(string, 1, fsize, f);
    string[read_bytes] = 0;
    fclose(f);
    cJSON_Delete(json);
    return string;
}

static motifgpt_tool_t tools[] = {
    {
        "read_file",
        "Read the contents of a file on the local filesystem.",
        "{\"type\": \"object\", \"properties\": {\"filename\": {\"type\": \"string\", \"description\": \"The path to the file to read\"}}, \"required\": [\"filename\"]}",
        filereader_execute
    }
};

static motifgpt_plugin_t plugin = {
    "FileReaderPlugin",
    tools,
    1
};

motifgpt_plugin_t* motifgpt_plugin_init(void) {
    return &plugin;
}
