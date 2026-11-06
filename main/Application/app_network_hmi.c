#include "app_network_hmi.h"

#include "cJSON.h"

#include <stdio.h>
#include <string.h>

static void copy_trunc(char *dst, size_t dst_len, const char *src)
{
    if (!dst || dst_len == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    snprintf(dst, dst_len, "%s", src);
}

void app_network_hmi_view_clear(app_network_hmi_view_t *view)
{
    if (!view) {
        return;
    }
    memset(view, 0, sizeof(*view));
}

bool app_network_hmi_parse(const char *json, size_t json_len, app_network_hmi_view_t *view)
{
    if (!view) {
        return false;
    }
    app_network_hmi_view_clear(view);
    if (!json || json_len == 0) {
        return false;
    }
    (void)json_len;

    cJSON *root = cJSON_Parse(json);
    if (!root || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return false;
    }

    const cJSON *jtitle = cJSON_GetObjectItem(root, "title");
    if (cJSON_IsString(jtitle) && jtitle->valuestring) {
        copy_trunc(view->title, sizeof(view->title), jtitle->valuestring);
    }

    const cJSON *jlines = cJSON_GetObjectItem(root, "lines");
    if (cJSON_IsArray(jlines)) {
        const int n = cJSON_GetArraySize(jlines);
        for (int i = 0; i < n && view->line_count < APP_NETWORK_HMI_LINE_MAX; i++) {
            const cJSON *el = cJSON_GetArrayItem(jlines, i);
            if (cJSON_IsString(el) && el->valuestring) {
                copy_trunc(view->lines[view->line_count], APP_NETWORK_HMI_LINE_LEN, el->valuestring);
                view->line_count++;
            }
        }
    }

    const cJSON *jitems = cJSON_GetObjectItem(root, "items");
    if (cJSON_IsArray(jitems)) {
        const int n = cJSON_GetArraySize(jitems);
        for (int i = 0; i < n && view->item_count < APP_NETWORK_HMI_ITEM_MAX; i++) {
            const cJSON *el = cJSON_GetObjectItem(jitems, i);
            if (!cJSON_IsObject(el)) {
                continue;
            }
            const cJSON *jl = cJSON_GetObjectItem(el, "label");
            const cJSON *jv = cJSON_GetObjectItem(el, "value");
            if (!cJSON_IsString(jl) || !cJSON_IsString(jv)) {
                continue;
            }
            app_network_hmi_item_t *it = &view->items[view->item_count];
            copy_trunc(it->label, sizeof(it->label), jl->valuestring);
            copy_trunc(it->value, sizeof(it->value), jv->valuestring);
            view->item_count++;
        }
    }

    cJSON_Delete(root);

    return view->title[0] != '\0' || view->line_count > 0 || view->item_count > 0;
}
