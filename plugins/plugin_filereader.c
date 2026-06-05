#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <cjson/cJSON.h>
#include <Xm/MessageB.h>
#include "motifgpt_plugin.h"

#define MAX_FILE_SIZE (1024 * 1024) // 1MB limit

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
        char err_buf[512];
        snprintf(err_buf, sizeof(err_buf), "Error: Could not open file '%s': %s", file_param->valuestring, strerror(errno));
        cJSON_Delete(json);
        return strdup(err_buf);
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize > MAX_FILE_SIZE) {
        fclose(f);
        cJSON_Delete(json);
        return strdup("Error: File too large (limit is 1MB)");
    }

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

void filereader_settings(Widget parent) {
    Widget dialog = XmCreateInformationDialog(parent, "filereaderSettings", NULL, 0);
    XmString title = XmStringCreateLocalized("File Reader Plugin Settings");
    XmString msg = XmStringCreateLocalized("File reader plugin is enabled. It allows the LLM to read local files (up to 1MB) that you specify.");
    XtVaSetValues(dialog, XmNdialogTitle, title, XmNmessageString, msg, NULL);
    XmStringFree(title); XmStringFree(msg);
    XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_CANCEL_BUTTON));
    XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_HELP_BUTTON));
    XtManageChild(dialog);
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
    1,
    filereader_settings
};

motifgpt_plugin_t* motifgpt_plugin_init(void) {
    return &plugin;
}
