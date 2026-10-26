#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file app_tasks.h
 * @brief 应用 FreeRTOS 任务统一创建入口
 *
 * 当前任务：
 *   - lvgl：LVGL tick + lv_timer_handler + ui_common 界面
 *
 * 新增业务任务时：在 app_tasks.c 增加启动函数，并在 app_tasks_start() 中调用。
 */

/**
 * LVGL 任务参数（与 app_lvgl.c 中 xTaskCreateWithCaps 一致）
 * usStackDepth 单位为 FreeRTOS word（ESP32 上通常 4 字节/word）
 */
#define APP_TASK_LVGL_NAME        "lvgl"
#define APP_TASK_LVGL_STACK_WORDS (48 * 1024)
#define APP_TASK_LVGL_PRIO        4

/**
 * 创建所有应用任务（须 app_init() 成功后调用）。
 * 可重复调用：若已启动则返回 ESP_OK。
 */
esp_err_t app_tasks_start(void);

/** 应用任务是否已启动 */
bool app_tasks_is_started(void);

#ifdef __cplusplus
}
#endif
