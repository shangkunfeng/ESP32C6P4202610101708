#include "app_wifi.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "app_wifi";

static bool s_ready;
static bool s_connected;
static bool s_scanning;
static bool s_connecting;
static char s_ip[16];
static char s_conn_ssid[33];

static app_wifi_ap_record_t s_scan[APP_WIFI_SCAN_MAX];
static size_t s_scan_count;

static SemaphoreHandle_t s_lock;
static app_wifi_ui_notify_fn s_ui_notify;
static void *s_ui_notify_ud;

static esp_netif_t *s_sta_netif;

static void notify_ui(void)
{
    if (s_ui_notify) {
        s_ui_notify(s_ui_notify_ud);
    }
}

static int cmp_ap_rssi(const void *a, const void *b)
{
    const app_wifi_ap_record_t *ap_a = a;
    const app_wifi_ap_record_t *ap_b = b;
    return (int)ap_b->rssi - (int)ap_a->rssi;
}

static void copy_scan_results(void)
{
    uint16_t ap_num = 0;
    if (esp_wifi_scan_get_ap_num(&ap_num) != ESP_OK) {
        s_scan_count = 0;
        return;
    }
    if (ap_num == 0) {
        s_scan_count = 0;
        return;
    }

    uint16_t fetch = ap_num;
    wifi_ap_record_t *raw = calloc(fetch, sizeof(wifi_ap_record_t));
    if (!raw) {
        ESP_LOGE(TAG, "scan calloc failed");
        s_scan_count = 0;
        return;
    }

    if (esp_wifi_scan_get_ap_records(&fetch, raw) != ESP_OK) {
        free(raw);
        s_scan_count = 0;
        return;
    }

    s_scan_count = 0;
    for (uint16_t i = 0; i < fetch; i++) {
        if (raw[i].ssid[0] == '\0') {
            continue;
        }
        const char *ssid = (const char *)raw[i].ssid;
        int existing = -1;
        for (size_t j = 0; j < s_scan_count; j++) {
            if (strcmp(s_scan[j].ssid, ssid) == 0) {
                existing = (int)j;
                break;
            }
        }
        if (existing >= 0) {
            app_wifi_ap_record_t *dst = &s_scan[existing];
            if (raw[i].rssi > dst->rssi) {
                dst->rssi = raw[i].rssi;
                dst->auth = raw[i].authmode;
            }
            continue;
        }
        if (s_scan_count >= APP_WIFI_SCAN_MAX) {
            continue;
        }
        app_wifi_ap_record_t *dst = &s_scan[s_scan_count];
        memcpy(dst->ssid, raw[i].ssid, sizeof(dst->ssid));
        dst->ssid[32] = '\0';
        dst->rssi = raw[i].rssi;
        dst->auth = raw[i].authmode;
        s_scan_count++;
    }
    qsort(s_scan, s_scan_count, sizeof(s_scan[0]), cmp_ap_rssi);
    free(raw);
}

static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)base;

    switch (id) {
    case WIFI_EVENT_SCAN_DONE:
        if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(500)) == pdTRUE) {
            s_scanning = false;
            copy_scan_results();
            xSemaphoreGive(s_lock);
        }
        ESP_LOGI(TAG, "scan done, %u AP(s)", (unsigned)s_scan_count);
        notify_ui();
        break;
    case WIFI_EVENT_STA_START:
        ESP_LOGI(TAG, "STA start");
        break;
    case WIFI_EVENT_STA_CONNECTED: {
        s_connected = true;
        s_connecting = false;
        wifi_config_t cfg;
        if (esp_wifi_get_config(WIFI_IF_STA, &cfg) == ESP_OK) {
            strncpy(s_conn_ssid, (const char *)cfg.sta.ssid, sizeof(s_conn_ssid) - 1);
        }
        ESP_LOGI(TAG, "STA connected");
        notify_ui();
        break;
    }
    case WIFI_EVENT_STA_DISCONNECTED: {
        const wifi_event_sta_disconnected_t *disc = data;
        s_connected = false;
        s_connecting = false;
        s_ip[0] = '\0';
        ESP_LOGW(TAG, "STA disconnected reason=%d", disc ? (int)disc->reason : -1);
        notify_ui();
        break;
    }
    default:
        break;
    }
}

static void on_ip_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)base;
    if (id != IP_EVENT_STA_GOT_IP) {
        return;
    }
    const ip_event_got_ip_t *ev = data;
    if (!ev) {
        return;
    }
    snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&ev->ip_info.ip));
    s_connected = true;
    s_connecting = false;
    ESP_LOGI(TAG, "got IP %s", s_ip);
    notify_ui();
}

static void wifi_connect_task(void *param)
{
    char ssid[33];
    char pass[65];
    memcpy(ssid, param, 33);
    memcpy(pass, (char *)param + 33, 65);
    free(param);

    wifi_config_t cfg = { 0 };
    strncpy((char *)cfg.sta.ssid, ssid, sizeof(cfg.sta.ssid) - 1);
    strncpy((char *)cfg.sta.password, pass, sizeof(cfg.sta.password) - 1);
    cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set_config: %s", esp_err_to_name(err));
        s_connecting = false;
        notify_ui();
        vTaskDelete(NULL);
        return;
    }
    strncpy(s_conn_ssid, ssid, sizeof(s_conn_ssid) - 1);
    err = esp_wifi_connect();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "connect: %s", esp_err_to_name(err));
        s_connecting = false;
        notify_ui();
    }
    vTaskDelete(NULL);
}

bool app_wifi_ap_needs_password(const app_wifi_ap_record_t *ap)
{
    if (!ap) {
        return true;
    }
    return ap->auth != WIFI_AUTH_OPEN && ap->auth != WIFI_AUTH_OWE;
}

esp_err_t app_wifi_init(void)
{
    if (s_ready) {
        return ESP_OK;
    }

    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        return err;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    if (!s_sta_netif) {
        s_sta_netif = esp_netif_create_default_wifi_sta();
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init: %s", esp_err_to_name(err));
        return err;
    }

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &on_wifi_event, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_ip_event, NULL, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    s_ready = true;
    ESP_LOGI(TAG, "Wi-Fi STA ready (C6 remote)");
    return ESP_OK;
}

bool app_wifi_is_ready(void)
{
    return s_ready;
}

bool app_wifi_is_connected(void)
{
    return s_connected;
}

bool app_wifi_is_scanning(void)
{
    return s_scanning;
}

bool app_wifi_is_connecting(void)
{
    return s_connecting;
}

esp_err_t app_wifi_get_ip_str(char *buf, size_t len)
{
    if (!buf || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_connected || s_ip[0] == '\0') {
        snprintf(buf, len, "0.0.0.0");
        return ESP_OK;
    }
    snprintf(buf, len, "%s", s_ip);
    return ESP_OK;
}

esp_err_t app_wifi_get_connected_ssid(char *buf, size_t len)
{
    if (!buf || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_connected || s_conn_ssid[0] == '\0') {
        buf[0] = '\0';
        return ESP_OK;
    }
    snprintf(buf, len, "%s", s_conn_ssid);
    return ESP_OK;
}

esp_err_t app_wifi_start_scan(void)
{
    if (!s_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_scanning) {
        return ESP_ERR_INVALID_STATE;
    }
    s_scanning = true;
    esp_wifi_scan_stop();
    wifi_scan_config_t scan_cfg = {
        .show_hidden = false,
    };
    esp_err_t err = esp_wifi_scan_start(&scan_cfg, false);
    if (err != ESP_OK) {
        s_scanning = false;
        ESP_LOGE(TAG, "scan_start: %s", esp_err_to_name(err));
    }
    return err;
}

size_t app_wifi_scan_count(void)
{
    return s_scan_count;
}

const app_wifi_ap_record_t *app_wifi_scan_get(size_t index)
{
    if (index >= s_scan_count) {
        return NULL;
    }
    return &s_scan[index];
}

esp_err_t app_wifi_connect(const char *ssid, const char *password)
{
    if (!s_ready || !ssid || ssid[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_connecting) {
        return ESP_ERR_INVALID_STATE;
    }

    char *buf = malloc(33 + 65);
    if (!buf) {
        return ESP_ERR_NO_MEM;
    }
    memset(buf, 0, 33 + 65);
    strncpy(buf, ssid, 32);
    if (password) {
        strncpy(buf + 33, password, 64);
    }

    s_connecting = true;
    notify_ui();

    if (xTaskCreate(wifi_connect_task, "wifi_conn", 4096, buf, 5, NULL) != pdPASS) {
        free(buf);
        s_connecting = false;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void app_wifi_set_ui_notify(app_wifi_ui_notify_fn fn, void *user_data)
{
    s_ui_notify = fn;
    s_ui_notify_ud = user_data;
}
