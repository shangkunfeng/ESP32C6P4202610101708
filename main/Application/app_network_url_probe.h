#pragma once

#include "esp_err.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*app_network_url_probe_done_fn)(void *user_data, bool success, int http_status);

/** 后台 HTTP GET 探测；同一时刻仅允许一次。成功：链路 up 且 HTTP 2xx/3xx。 */
esp_err_t app_network_url_probe_start(const char *url, app_network_url_probe_done_fn cb, void *user_data);

/** 离开 ui_network 时调用，忽略进行中的结果。 */
void app_network_url_probe_cancel(void);

bool app_network_url_probe_busy(void);

#ifdef __cplusplus
}
#endif
