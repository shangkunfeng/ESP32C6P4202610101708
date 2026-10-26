#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 检查 photo/ui_login.bin、ui_home_back.bin、ui_home.bin */
esp_err_t app_sd_background_probe(void);

/** 从 SD 读入上述全屏图到 PSRAM */
esp_err_t app_sd_background_load_psram(void);

bool app_sd_background_is_ready(void);

const uint16_t *app_sd_background_get_ui_login_rgb565(void);

const uint16_t *app_sd_background_get_ui_home_back_rgb565(void);

const uint16_t *app_sd_background_get_ui_home_rgb565(void);

/** SD /sdcard/images/wifi_*.bin（20×20 RGB565），未加载则 NULL */
const uint16_t *app_sd_background_get_wifi_no_link_rgb565(void);

const uint16_t *app_sd_background_get_wifi_link_rgb565(void);

#ifdef __cplusplus
}
#endif
