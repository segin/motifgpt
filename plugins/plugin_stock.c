#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>
#include <Xm/Xm.h>
#include <Xm/Label.h>
#include <Xm/TextF.h>
#include <Xm/Form.h>
#include <Xm/PushB.h>
#include "motifgpt_plugin.h"

static char alpha_vantage_key[128] = "demo";

struct string {
    char *ptr;
    size_t len;
};

void init_string(struct string *s) {
    s->len = 0;
    s->ptr = malloc(s->len + 1);
    if (s->ptr == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    s->ptr[0] = '\0';
}

size_t writefunc(void *ptr, size_t size, size_t nmemb, struct string *s) {
    size_t new_len = s->len + size * nmemb;
    s->ptr = realloc(s->ptr, new_len + 1);
    if (s->ptr == NULL) {
        perror("realloc");
        exit(EXIT_FAILURE);
    }
    memcpy(s->ptr + s->len, ptr, size * nmemb);
    s->ptr[new_len] = '\0';
    s->len = new_len;
    return size * nmemb;
}

char* http_get(const char* url) {
    CURL *curl;
    CURLcode res;
    struct string s;
    init_string(&s);

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &s);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "MotifGPT-Plugin/1.0");
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            free(s.ptr);
            s.ptr = NULL;
        }
        curl_easy_cleanup(curl);
    }
    return s.ptr;
}

char* stock_execute(const char* args_json) {
    cJSON* json = cJSON_Parse(args_json);
    if (!json) return strdup("Error: Invalid JSON arguments");

    cJSON* symbol_param = cJSON_GetObjectItemCaseSensitive(json, "symbol");
    if (!cJSON_IsString(symbol_param) || (symbol_param->valuestring == NULL)) {
        cJSON_Delete(json);
        return strdup("Error: Missing 'symbol' string parameter");
    }

    char url[512];
    snprintf(url, sizeof(url), "https://www.alphavantage.co/query?function=GLOBAL_QUOTE&symbol=%s&apikey=%s", symbol_param->valuestring, alpha_vantage_key);

    char* resp = http_get(url);
    if (!resp) {
        cJSON_Delete(json);
        return strdup("Error: API request failed");
    }

    cJSON* resp_json = cJSON_Parse(resp);
    free(resp);
    if (!resp_json) {
        cJSON_Delete(json);
        return strdup("Error: Failed to parse API response");
    }

    cJSON* quote = cJSON_GetObjectItemCaseSensitive(resp_json, "Global Quote");
    if (!cJSON_IsObject(quote) || cJSON_GetArraySize(quote) == 0) {
        cJSON* note = cJSON_GetObjectItemCaseSensitive(resp_json, "Note");
        if (note) {
             cJSON_Delete(resp_json);
             cJSON_Delete(json);
             return strdup("Error: Alpha Vantage API limit reached or invalid key.");
        }
        cJSON_Delete(resp_json);
        cJSON_Delete(json);
        return strdup("Error: Symbol not found or invalid API response.");
    }

    const char* price = cJSON_GetObjectItemCaseSensitive(quote, "05. price")->valuestring;
    const char* change = cJSON_GetObjectItemCaseSensitive(quote, "10. change percent")->valuestring;

    char* result_msg = malloc(512);
    snprintf(result_msg, 512, "Stock info for %s: Price: $%s, Change: %s", symbol_param->valuestring, price ? price : "N/A", change ? change : "N/A");

    cJSON_Delete(resp_json);
    cJSON_Delete(json);
    return result_msg;
}

static Widget key_text;

void stock_save_callback(Widget w, XtPointer client_data, XtPointer call_data) {
    char* val = XmTextFieldGetString(key_text);
    if (val) {
        strncpy(alpha_vantage_key, val, sizeof(alpha_vantage_key)-1);
        alpha_vantage_key[sizeof(alpha_vantage_key)-1] = '\0';
        XtFree(val);
    }
    XtUnmanageChild((Widget)client_data);
}

void stock_settings(Widget parent) {
    Widget shell = XtVaCreatePopupShell("Stock Settings", transientShellWidgetClass, parent, NULL);
    Widget form = XtVaCreateManagedWidget("stockForm", xmFormWidgetClass, shell, XmNmarginWidth, 10, XmNmarginHeight, 10, NULL);
    
    Widget label = XtVaCreateManagedWidget("Alpha Vantage API Key:", xmLabelWidgetClass, form,
                                           XmNtopAttachment, XmATTACH_FORM,
                                           XmNleftAttachment, XmATTACH_FORM, NULL);
    
    key_text = XtVaCreateManagedWidget("keyText", xmTextFieldWidgetClass, form,
                                       XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, label,
                                       XmNleftAttachment, XmATTACH_FORM,
                                       XmNrightAttachment, XmATTACH_FORM, NULL);
    XmTextFieldSetString(key_text, alpha_vantage_key);

    Widget ok_btn = XtVaCreateManagedWidget("Save", xmPushButtonWidgetClass, form,
                                            XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, key_text,
                                            XmNtopOffset, 10,
                                            XmNrightAttachment, XmATTACH_FORM, NULL);
    XtAddCallback(ok_btn, XmNactivateCallback, stock_save_callback, (XtPointer)shell);

    XtManageChild(shell);
}

static motifgpt_tool_t tools[] = {
    {
        "get_stock_price",
        "Get the current stock price for a given ticker symbol using Alpha Vantage.",
        "{\"type\": \"object\", \"properties\": {\"symbol\": {\"type\": \"string\", \"description\": \"The stock ticker symbol, e.g., AAPL\"}}, \"required\": [\"symbol\"]}",
        stock_execute
    }
};

static motifgpt_plugin_t plugin = {
    "StockPlugin",
    tools,
    1,
    stock_settings
};

motifgpt_plugin_t* motifgpt_plugin_init(void) {
    return &plugin;
}
