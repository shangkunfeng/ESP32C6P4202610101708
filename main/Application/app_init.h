#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file app_init.h
 * @brief 应用层一次性初始化（板级 → 显示/触摸 → SD → 背景资源）
 *
 * 调用顺序（由 app_init 内部保证）：
 *   1. board_display_hw_init
 *   2. driver_lcd_init / driver_touch_init
 *   3. driver_sd_tf_init
 *   4. app_sd_background_probe / app_sd_background_load_psram
 *
 * SD/资源失败时会在 LCD 上显示错误色（见 app_init.c）。
 */

/** 初始化阶段（用于日志或后续扩展状态查询） */
typedef enum {
    APP_INIT_PHASE_NONE = 0,
    APP_INIT_PHASE_BOARD,
    APP_INIT_PHASE_DISPLAY,
    APP_INIT_PHASE_TOUCH,
    APP_INIT_PHASE_STORAGE,
    APP_INIT_PHASE_ASSETS,
    APP_INIT_PHASE_DONE,
} app_init_phase_t;

/**
 * 执行完整应用初始化。
 * @return ESP_OK 成功；否则失败（部分步骤已显示错误画面）
 */
esp_err_t app_init(void);

/** 是否已成功完成 app_init() */
bool app_init_is_done(void);

/** 当前/最近一次完成的初始化阶段 */
app_init_phase_t app_init_get_phase(void);

#ifdef __cplusplus
}
#endif
