#include "app_init.h"

#include "app_history_store.h"
#include "app_param_config.h"
#include "app_sd_background.h"
#include "board_display.h"
#include "display_config.h"
#include "driver_lcd.h"
#include "driver_sd_tf.h"
#include "driver_touch.h"
#include "sd_tf_config.h"

#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "app_init";

static app_init_phase_t s_phase = APP_INIT_PHASE_NONE;
static bool s_init_done;

static void show_init_error_screen(uint16_t rgb565)
{
    esp_err_t err = driver_lcd_fill_screen(rgb565);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "error screen fill failed: %s", esp_err_to_name(err));
    }
}

static esp_err_t init_board(void)
{
    s_phase = APP_INIT_PHASE_BOARD;
    ESP_LOGI(TAG, "[1/4] board (backlight / MIPI PHY 等)");
    return board_display_hw_init();
}

static esp_err_t init_display_driver(void)
{
    s_phase = APP_INIT_PHASE_DISPLAY;
    ESP_LOGI(TAG, "[2/4] LCD JD9165 %dx%d RGB565", LCD_H_RES, LCD_V_RES);
    return driver_lcd_init();
}

static esp_err_t init_touch_driver(void)
{
    s_phase = APP_INIT_PHASE_TOUCH;
    ESP_LOGI(TAG, "[3/4] touch GT911");
    return driver_touch_init();
}

static esp_err_t init_storage(void)
{
    s_phase = APP_INIT_PHASE_STORAGE;
    ESP_LOGI(TAG, "[4/4] SD mount @ %s", TF_VFS_MOUNT_POINT);
    return driver_sd_tf_init();
}

static esp_err_t init_background_assets(void)
{
    s_phase = APP_INIT_PHASE_ASSETS;

    esp_err_t probe = app_sd_background_probe();
    if (probe != ESP_OK) {
        ESP_LOGE(TAG, "background probe failed: %s", esp_err_to_name(probe));
        ESP_LOGE(TAG, "需要 %s/%s/ui_login.bin ui_home_back.bin ui_home.bin",
                 TF_VFS_MOUNT_POINT, TF_PHOTO_DIR_NAME);
        show_init_error_screen(LCD_COLOR_BLUE);
        return probe;
    }

    esp_err_t cache = app_sd_background_load_psram();
    if (cache != ESP_OK) {
        ESP_LOGE(TAG, "PSRAM cache failed: %s", esp_err_to_name(cache));
        show_init_error_screen(LCD_COLOR_BLUE);
        return cache;
    }

    if (!app_sd_background_is_ready()) {
        ESP_LOGE(TAG, "background not ready after load");
        show_init_error_screen(LCD_COLOR_BLUE);
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "UI backgrounds in PSRAM (%s/%s/)", TF_VFS_MOUNT_POINT, TF_PHOTO_DIR_NAME);
    return ESP_OK;
}

bool app_init_is_done(void)
{
    return s_init_done;
}

app_init_phase_t app_init_get_phase(void)
{
    return s_phase;
}

esp_err_t app_init(void)
{
    if (s_init_done) {
        ESP_LOGW(TAG, "app_init already done, skip");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "======== application init start ========");

    esp_err_t err = init_board();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "board init: %s", esp_err_to_name(err));
        return err;
    }

    err = init_display_driver();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "display init: %s", esp_err_to_name(err));
        return err;
    }

    err = init_touch_driver();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "touch init: %s", esp_err_to_name(err));
        return err;
    }

    err = init_storage();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SD init: %s", esp_err_to_name(err));
        show_init_error_screen(LCD_COLOR_RED);
        return err;
    }

    err = param_config_load();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "param load: %s (factory defaults)", esp_err_to_name(err));
    }
    param_config_stop_all_channels();
    ESP_LOGI(TAG, "history SD write off at boot (all channel En=false)");

    err = app_history_store_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "history store init: %s", esp_err_to_name(err));
    }

    err = init_background_assets();
    if (err != ESP_OK) {
        return err;
    }

    s_phase = APP_INIT_PHASE_DONE;
    s_init_done = true;
    ESP_LOGI(TAG, "======== application init done ========");
    return ESP_OK;
}
