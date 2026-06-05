#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>
#include <Xm/MessageB.h>
#include "motifgpt_plugin.h"

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

char* weather_execute(const char* args_json) {
    cJSON* json = cJSON_Parse(args_json);
    if (!json) return strdup("Error: Invalid JSON arguments");

    cJSON* location_param = cJSON_GetObjectItemCaseSensitive(json, "location");
    if (!cJSON_IsString(location_param) || (location_param->valuestring == NULL)) {
        cJSON_Delete(json);
        return strdup("Error: Missing 'location' string parameter");
    }

    // 1. Geocoding
    char geocode_url[512];
    char* encoded_loc = curl_easy_escape(NULL, location_param->valuestring, 0);
    snprintf(geocode_url, sizeof(geocode_url), "https://geocoding-api.open-meteo.com/v1/search?name=%s&count=1&language=en&format=json", encoded_loc);
    curl_free(encoded_loc);

    char* geocode_resp = http_get(geocode_url);
    if (!geocode_resp) {
        cJSON_Delete(json);
        return strdup("Error: Geocoding request failed");
    }

    cJSON* geo_json = cJSON_Parse(geocode_resp);
    free(geocode_resp);
    if (!geo_json) {
        cJSON_Delete(json);
        return strdup("Error: Failed to parse geocoding response");
    }

    cJSON* results = cJSON_GetObjectItemCaseSensitive(geo_json, "results");
    if (!cJSON_IsArray(results) || cJSON_GetArraySize(results) == 0) {
        cJSON_Delete(geo_json);
        cJSON_Delete(json);
        return strdup("Error: Location not found");
    }

    cJSON* first_result = cJSON_GetArrayItem(results, 0);
    double lat = cJSON_GetObjectItemCaseSensitive(first_result, "latitude")->valuedouble;
    double lon = cJSON_GetObjectItemCaseSensitive(first_result, "longitude")->valuedouble;
    const char* name = cJSON_GetObjectItemCaseSensitive(first_result, "name")->valuestring;

    // 2. Weather
    char weather_url[512];
    snprintf(weather_url, sizeof(weather_url), "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current_weather=true", lat, lon);
    
    char* weather_resp = http_get(weather_url);
    if (!weather_resp) {
        cJSON_Delete(geo_json);
        cJSON_Delete(json);
        return strdup("Error: Weather request failed");
    }

    cJSON* weather_json = cJSON_Parse(weather_resp);
    free(weather_resp);
    if (!weather_json) {
        cJSON_Delete(geo_json);
        cJSON_Delete(json);
        return strdup("Error: Failed to parse weather response");
    }

    cJSON* current = cJSON_GetObjectItemCaseSensitive(weather_json, "current_weather");
    double temp = cJSON_GetObjectItemCaseSensitive(current, "temperature")->valuedouble;
    double windspeed = cJSON_GetObjectItemCaseSensitive(current, "windspeed")->valuedouble;

    char* result_msg = malloc(512);
    snprintf(result_msg, 512, "Current weather in %s: %.1f°C, Wind Speed: %.1f km/h", name, temp, windspeed);

    cJSON_Delete(weather_json);
    cJSON_Delete(geo_json);
    cJSON_Delete(json);
    return result_msg;
}

void weather_settings(Widget parent) {
    Widget dialog = XmCreateInformationDialog(parent, "weatherSettings", NULL, 0);
    XmString title = XmStringCreateLocalized("Weather Plugin Settings");
    XmString msg = XmStringCreateLocalized("Weather plugin uses Open-Meteo API (No API key required for non-commercial use).");
    XtVaSetValues(dialog, XmNdialogTitle, title, XmNmessageString, msg, NULL);
    XmStringFree(title); XmStringFree(msg);
    XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_CANCEL_BUTTON));
    XtUnmanageChild(XmMessageBoxGetChild(dialog, XmDIALOG_HELP_BUTTON));
    XtManageChild(dialog);
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
    1,
    weather_settings
};

motifgpt_plugin_t* motifgpt_plugin_init(void) {
    return &plugin;
}
