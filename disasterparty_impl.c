#include "disasterparty.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <cjson/cJSON.h>

int dp_serialize_messages_to_fd(dp_message_t *msgs, size_t count, int fd) {
    if (!msgs || fd < 0) return -1;

    cJSON *root = cJSON_CreateArray();
    if (!root) return -1;

    for (size_t i = 0; i < count; i++) {
        cJSON *msg_obj = cJSON_CreateObject();
        if (!msg_obj) { cJSON_Delete(root); return -1; }
        cJSON_AddItemToArray(root, msg_obj);

        const char *role_str = (msgs[i].role == DP_ROLE_USER) ? "user" : "assistant";
        cJSON_AddStringToObject(msg_obj, "role", role_str);

        cJSON *parts_arr = cJSON_CreateArray();
        if (!parts_arr) { cJSON_Delete(root); return -1; }
        cJSON_AddItemToObject(msg_obj, "parts", parts_arr);

        for (size_t j = 0; j < msgs[i].num_parts; j++) {
            cJSON *part_obj = cJSON_CreateObject();
            if (!part_obj) { cJSON_Delete(root); return -1; }
            cJSON_AddItemToArray(parts_arr, part_obj);

            if (msgs[i].parts[j].type == DP_CONTENT_PART_TEXT) {
                cJSON_AddStringToObject(part_obj, "type", "text");
                cJSON_AddStringToObject(part_obj, "text", msgs[i].parts[j].text ? msgs[i].parts[j].text : "");
            } else if (msgs[i].parts[j].type == DP_CONTENT_PART_IMAGE_BASE64) {
                cJSON_AddStringToObject(part_obj, "type", "image");
                cJSON_AddStringToObject(part_obj, "mime_type", msgs[i].parts[j].image_mime_type ? msgs[i].parts[j].image_mime_type : "");
                cJSON_AddStringToObject(part_obj, "data", msgs[i].parts[j].image_base64_data ? msgs[i].parts[j].image_base64_data : "");
            }
        }
    }

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!json_str) return -1;

    size_t len = strlen(json_str);
    ssize_t written = write(fd, json_str, len);
    free(json_str);

    return (written == (ssize_t)len) ? 0 : -1;
}

int dp_serialize_messages_to_file(dp_message_t *msgs, size_t count, const char *filename) {
    if (!filename) return -1;
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) return -1;
    int ret = dp_serialize_messages_to_fd(msgs, count, fd);
    close(fd);
    return ret;
}
