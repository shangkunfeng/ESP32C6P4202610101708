#pragma once

#include "esp_err.h"
#include <stdint.h>

/**
 * 板级显示：背光 PWM + LCD 硬件复位（JD9165 / FPC）
 * 不含 MIPI 驱动与 LVGL（后续 Driver / Middleware）
 */

/** 初始化背光 LEDC、配置 RST GPIO 为默认释放态 */
esp_err_t board_display_hw_init(void);

/** 背光亮度 30~100（%） */
esp_err_t board_set_backlight(uint32_t brightness_percent);

/** 当前背光 30–100（与 board_set_backlight 一致） */
uint32_t board_get_backlight(void);

/** LCD_RST 脉冲，时序对齐 TouchPanel esp_lcd_jd9165 硬件复位 */
esp_err_t board_lcd_reset_pulse(void);
