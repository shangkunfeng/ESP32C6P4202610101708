#include "app_tasks.h"

#include "app_history_store.h"
#include "app_init.h"
#include "app_lvgl.h"
#include "app_mic.h"
#include "app_uart1_plot.h"
#include "app_network_config.h"
#if APP_WIFI_TASK_ENABLE
#include "app_wifi.h"
#endif
#if APP_ETH_TASK_ENABLE
#include "app_eth.h"
#endif
#include "uart1_config.h"
#include "driver_es8311.h"
#include "sd_tf_config.h"

#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "app_tasks";

static bool s_tasks_started;

static esp_err_t start_lvgl_task(void)
{
    ESP_LOGI(TAG, "create task \"%s\" stack=%u prio=%u",
             APP_TASK_LVGL_NAME,
             (unsigned)APP_TASK_LVGL_STACK_WORDS,
             (unsigned)APP_TASK_LVGL_PRIO);

    return app_lvgl_start();
}

bool app_tasks_is_started(void)
{
    return s_tasks_started;
}

esp_err_t app_tasks_start(void)
{
    if (s_tasks_started) {
        ESP_LOGW(TAG, "app_tasks_start already called, skip");
        return ESP_OK;
    }

    ESP_RETURN_ON_FALSE(app_init_is_done(), ESP_ERR_INVALID_STATE, TAG,
                        "call app_init() first");

    ESP_LOGI(TAG, "======== application tasks start ========");

#if APP_WIFI_TASK_ENABLE
    esp_err_t wifi_err = app_wifi_init();
    if (wifi_err != ESP_OK) {
        ESP_LOGW(TAG, "app_wifi_init: %s (ui_wifi scan/connect disabled)", esp_err_to_name(wifi_err));
    }
#else
    ESP_LOGI(TAG, "Wi-Fi task disabled (APP_WIFI_TASK_ENABLE=0)");
#endif

#if APP_ETH_TASK_ENABLE
    ESP_LOGI(TAG, "starting app_eth_init...");
    esp_err_t eth_err = app_eth_init();
    if (eth_err != ESP_OK) {
        ESP_LOGE(TAG, "app_eth_init failed: %s", esp_err_to_name(eth_err));
    }
#endif

    ESP_RETURN_ON_ERROR(start_lvgl_task(), TAG, "LVGL task");

    if (driver_es8311_is_inited()) {
        esp_err_t mic_err = app_mic_start();
        if (mic_err != ESP_OK) {
            ESP_LOGW(TAG, "mic task: %s", esp_err_to_name(mic_err));
        }
    }

#if UART1_SERIAL_STUDIO_PLOT_ENABLE
    {
        esp_err_t plot_err = app_uart1_plot_start();
        if (plot_err != ESP_OK) {
            ESP_LOGW(TAG, "uart1 quick plot: %s", esp_err_to_name(plot_err));
        }
    }
#endif

    esp_err_t hist_err = app_history_store_start();
    if (hist_err != ESP_OK) {
        ESP_LOGW(TAG, "history sampler: %s", esp_err_to_name(hist_err));
    }

    s_tasks_started = true;

    ESP_LOGI(TAG, "tasks running: ui_login + ui_home + ui_mode/lvgl_6 + trend + alarm + param (%s/%s)",
             TF_VFS_MOUNT_POINT, TF_PHOTO_DIR_NAME);
    ESP_LOGI(TAG, "======== application tasks ready ========");
    return ESP_OK;
}
