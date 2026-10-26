#pragma once

/**
 * JC1060P470C / JC-ESP32P4-M3 显示参数（仅屏，不含触摸）
 * 与 TouchPanel pins_config.h / 原理图 DSI_LCD 一致
 */

/* 分辨率 / 像素格式 */
#define LCD_H_RES                   1024
#define LCD_V_RES                   600
#define LCD_BIT_PER_PIXEL           16   /* RGB565 */

#define LCD_COLOR_RGB565            1

/* 未连接脚 */
#define PIN_NC                      (-1)

/* 屏复位：FPC LCD_RST → GPIO0，低有效 */
#define LCD_RST_GPIO                0
#define LCD_RST_ACTIVE_HIGH         0

/* 背光 PWM：LCD_PWM → MP3202，Demo GPIO23 */
#define LCD_BL_PWM_GPIO             23
#define LCD_BL_LEDC_TIMER_NUM       1
#define LCD_BL_LEDC_CH_NUM          0
#define LCD_BL_LEDC_FREQ_HZ         20000
#define LCD_BL_LEDC_DUTY_MAX        1023   /* 10-bit */

/** 背光 PWM 下限（%），UI 滑条与 board_set_backlight 均不低于此值 */
#define LCD_BL_MIN_PERCENT          30

/* MIPI DSI（专用脚，仅文档对照；由 esp_lcd 配置，非 GPIO bit-bang） */
#define MIPI_DSI_LANE_NUM           2

#define MIPI_DSI_MOD_PIN_CLK_P      29
#define MIPI_DSI_MOD_PIN_CLK_N      28
#define MIPI_DSI_MOD_PIN_DATA1_P    26
#define MIPI_DSI_MOD_PIN_DATA1_N    27
#define MIPI_DSI_MOD_PIN_DATA0_P    30
#define MIPI_DSI_MOD_PIN_DATA0_N    31

#define MIPI_DSI_PHY_LDO_CHAN       3
#define MIPI_DSI_PHY_LDO_MV         2500
