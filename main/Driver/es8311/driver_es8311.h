#ifndef __DRIVER_ES8311_H__
#define __DRIVER_ES8311_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** I2C + I2S（采集）；引脚见 es8311_config.h */
esp_err_t driver_es8311_init(void);

esp_err_t driver_es8311_deinit(void);

bool driver_es8311_is_inited(void);

/** 板载麦数字流经 ES7210→GPIO48，后续实现 */
esp_err_t driver_es8311_start_capture(void);

esp_err_t driver_es8311_stop_capture(void);

/**
 * 读取采集 PCM（stub 填 0）
 */
esp_err_t driver_es8311_read_pcm(int16_t *samples, size_t frame_count, uint8_t channels, size_t *frames_read);

#ifdef __cplusplus
}
#endif

#endif
