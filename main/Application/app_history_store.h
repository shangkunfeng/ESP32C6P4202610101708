#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 与 SD 目录 historydata/{co2,temp,humi,speed} 对应 */
typedef enum {
    APP_HIST_CH_CO2 = 0,
    APP_HIST_CH_TEMP,
    APP_HIST_CH_HUM,
    APP_HIST_CH_RPM,
    APP_HIST_CH_COUNT
} app_hist_channel_t;

#define APP_HIST_RECORD_SIZE        12U
#define APP_HIST_FILE_HEADER_SIZE   32U
#define APP_HIST_SAMPLE_INTERVAL_S  60U
#define APP_HIST_FLUSH_EVERY_N      30U
#define APP_HIST_RETENTION_DAYS     90U

/** SD 挂载成功后调用：创建目录、互斥量 */
esp_err_t app_history_store_init(void);

/** 启动后台采样写盘任务（app_tasks_start 中调用） */
esp_err_t app_history_store_start(void);

bool app_history_store_is_ready(void);

/**
 * 读取某通道最近 max_points 条记录（时间升序：旧→新）。
 * values_out / ts_out 长度至少 max_points。
 */
esp_err_t app_history_load_recent(app_hist_channel_t ch,
                                  uint32_t max_points,
                                  float *values_out,
                                  uint32_t *ts_out,
                                  size_t *count_out);

#ifdef __cplusplus
}
#endif
