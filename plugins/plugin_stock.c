#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "motifgpt_plugin.h"
#include <cjson/cJSON.h>

char* stock_execute(const char* args_json) {
    cJSON* json = cJSON_Parse(args_json);
    if (!json) return strdup("Error: Invalid JSON arguments");

    cJSON* symbol_param = cJSON_GetObjectItemCaseSensitive(json, "symbol");
    if (!cJSON_IsString(symbol_param) || (symbol_param->valuestring == NULL)) {
        cJSON_Delete(json);
        return strdup("Error: Missing 'symbol' string parameter");
    }

    // This is a mock implementation
    char result_buf[256];
    snprintf(result_buf, sizeof(result_buf), "The current price for %s is $150.00, up 2.5%% today.", symbol_param->valuestring);

    cJSON_Delete(json);
    return strdup(result_buf);
}

static motifgpt_tool_t tools[] = {
    {
        "get_stock_price",
        "Get the current stock price for a given ticker symbol.",
        "{\"type\": \"object\", \"properties\": {\"symbol\": {\"type\": \"string\", \"description\": \"The stock ticker symbol, e.g., AAPL\"}}, \"required\": [\"symbol\"]}",
        stock_execute
    }
};

static motifgpt_plugin_t plugin = {
    "StockPlugin",
    tools,
    1
};

motifgpt_plugin_t* motifgpt_plugin_init(void) {
    return &plugin;
}
