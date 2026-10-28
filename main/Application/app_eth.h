#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*app_eth_ui_notify_fn)(void *user_data);

/**
 * 有线以太网。Wi-Fi 关闭时由本模块初始化 NVS / esp_netif / 默认 event loop。
 */
esp_err_t app_eth_init(void);

bool app_eth_is_ready(void);

bool app_eth_is_link_up(void);

esp_err_t app_eth_get_ip_str(char *buf, size_t len);

void app_eth_set_ui_notify(app_eth_ui_notify_fn fn, void *user_data);

#ifdef __cplusplus
}
#endif
