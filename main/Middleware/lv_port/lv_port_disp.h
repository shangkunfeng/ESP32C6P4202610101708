#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * LVGL 9 显示移植（仅 disp，无触摸）。
 * 调用前须已完成 driver_lcd_init()（含双缓冲 + VSync）。
 */
esp_err_t lv_port_disp_init(void);

#ifdef __cplusplus
}
#endif
