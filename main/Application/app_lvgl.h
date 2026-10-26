#pragma once

#include "esp_err.h"
#include "lvgl.h"
#include "ui_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 与 ui_common 屏数量一致 */
#define APP_LVGL_SCREEN_COUNT UI_SCR_COUNT

/**
 * 创建 LVGL 任务；界面由 ui_common 懒加载切换。
 * 须在 app_sd_background_load_psram() 成功后调用。
 */
esp_err_t app_lvgl_start(void);

/** 切换 screen 0..9（内部调用 ui_change_screen） */
void app_lvgl_show_index(int index);

/** 为 screen 注册左/右滑切换（在 ui_lvgl_N_events_register 内调用） */
void app_lvgl_bind_swipe(lv_obj_t *screen, int screen_index);

#ifdef __cplusplus
}
#endif
