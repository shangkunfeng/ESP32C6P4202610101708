#pragma once

#include "app_network_hmi.h"
#include "esp_err.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*app_network_url_probe_done_fn)(void *user_data,
                                              bool success,
                                              int http_status,
                                              const app_network_hmi_view_t *hmi,
                                              bool hmi_parsed);

/**
 * 后台 HTTP GET：链路 up 且 2xx/3xx 为 success；
 * 响应体按 app_network_hmi JSON 解析，hmi_parsed 表示是否解析出界面数据。
 */
esp_err_t app_network_url_probe_start(const char *url, app_network_url_probe_done_fn cb, void *user_data);

void app_network_url_probe_cancel(void);

bool app_network_url_probe_busy(void);

#ifdef __cplusplus
}
#endif
