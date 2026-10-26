#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "sdmmc_cmd.h"

#ifdef __cplusplus
extern "C" {
#endif

/** LDO VO4 + SDMMC 4-bit，挂载 TF_VFS_MOUNT_POINT；重复调用返回 ESP_ERR_INVALID_STATE */
esp_err_t driver_sd_tf_init(void);

/** 卸载 VFS 并释放 LDO 电源控制（可选） */
esp_err_t driver_sd_tf_deinit(void);

/** 挂载成功后非 NULL */
sdmmc_card_t *driver_sd_tf_get_card(void);

/** 全卡唯一互斥：动画/历史/参数等不得并行访问 SDMMC */
bool driver_sd_tf_bus_lock(TickType_t timeout_ticks);
void driver_sd_tf_bus_unlock(void);

#ifdef __cplusplus
}
#endif
