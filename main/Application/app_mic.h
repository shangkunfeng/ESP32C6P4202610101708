#ifndef __APP_MIC_H__
#define __APP_MIC_H__

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 创建采麦任务（须 driver_es8311_init 已成功） */
esp_err_t app_mic_start(void);

/** 最近一帧 PCM 绝对值峰值，供 UART1 声音通道 */
int32_t app_mic_get_pcm_peak(void);

#ifdef __cplusplus
}
#endif

#endif
