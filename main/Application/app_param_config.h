#ifndef __APP_PARAM_CONFIG_H__
#define __APP_PARAM_CONFIG_H__

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 外挂 Flash 侧逻辑：出厂默认写入四个 driver 结构体（不擦写整片 Flash） */
void param_config_apply_factory_defaults(void);

/** 上电后四路均不运行（在 param_config_load 之后也会调用） */
void param_config_stop_all_channels(void);

/**
 * 开机调用：先出厂默认，若 SD 上 user_param.json 有效则覆盖（不含 Real_*）
 * SD 不可用或文件无效时保持默认。
 */
esp_err_t param_config_load(void);

/** 将当前四个结构体可配置字段写入 /sdcard/param/user_param.json */
esp_err_t param_config_save_to_sd(void);

/** 删除 SD 用户参数文件（重置默认后调用） */
esp_err_t param_config_remove_user_file(void);

#ifdef __cplusplus
}
#endif

#endif
