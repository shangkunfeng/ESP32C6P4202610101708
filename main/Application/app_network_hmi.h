#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 服务器 HMI JSON（GET 同一 URL，Content-Type 建议 application/json）示例：
 * {
 *   "title": "设备监控",
 *   "lines": ["产线 A 正常", "最后更新: 12:00"],
 *   "items": [
 *     {"label": "温度", "value": "25.6 C"},
 *     {"label": "转速", "value": "1200 rpm"}
 *   ]
 * }
 */
#define APP_NETWORK_HMI_TITLE_LEN   48
#define APP_NETWORK_HMI_LINE_LEN    72
#define APP_NETWORK_HMI_LINE_MAX    8
#define APP_NETWORK_HMI_ITEM_LABEL  32
#define APP_NETWORK_HMI_ITEM_VALUE  48
#define APP_NETWORK_HMI_ITEM_MAX    6

typedef struct {
    char label[APP_NETWORK_HMI_ITEM_LABEL];
    char value[APP_NETWORK_HMI_ITEM_VALUE];
} app_network_hmi_item_t;

typedef struct {
    char title[APP_NETWORK_HMI_TITLE_LEN];
    char lines[APP_NETWORK_HMI_LINE_MAX][APP_NETWORK_HMI_LINE_LEN];
    size_t line_count;
    app_network_hmi_item_t items[APP_NETWORK_HMI_ITEM_MAX];
    size_t item_count;
} app_network_hmi_view_t;

void app_network_hmi_view_clear(app_network_hmi_view_t *view);

/** 解析 JSON 文本；成功返回 true 且至少含 title、lines 或 items 之一。 */
bool app_network_hmi_parse(const char *json, size_t json_len, app_network_hmi_view_t *view);

#ifdef __cplusplus
}
#endif
