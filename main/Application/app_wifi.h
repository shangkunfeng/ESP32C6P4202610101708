#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"
#include "esp_wifi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_WIFI_SCAN_MAX   32

typedef struct {
    char ssid[33];
    int8_t rssi;
    wifi_auth_mode_t auth;
} app_wifi_ap_record_t;

typedef void (*app_wifi_ui_notify_fn)(void *user_data);

esp_err_t app_wifi_init(void);
bool app_wifi_is_ready(void);

bool app_wifi_is_connected(void);
bool app_wifi_is_scanning(void);
bool app_wifi_is_connecting(void);

esp_err_t app_wifi_get_ip_str(char *buf, size_t len);
esp_err_t app_wifi_get_connected_ssid(char *buf, size_t len);

esp_err_t app_wifi_start_scan(void);

size_t app_wifi_scan_count(void);
const app_wifi_ap_record_t *app_wifi_scan_get(size_t index);

bool app_wifi_ap_needs_password(const app_wifi_ap_record_t *ap);

esp_err_t app_wifi_connect(const char *ssid, const char *password);

void app_wifi_set_ui_notify(app_wifi_ui_notify_fn fn, void *user_data);

#ifdef __cplusplus
}
#endif
