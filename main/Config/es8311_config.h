#ifndef __ES8311_CONFIG_H__
#define __ES8311_CONFIG_H__

/**
 * JC1060P470C / JC-ESP32P4-M3 — ES8311 / 板载麦（采集）硬件参数
 *
 * 原理图 JC1060P470C_I_W_Y-V1.0（ES8311 页）：
 *   - U1 ES8311-S：I2C 配置 + I2S0 时钟/模拟麦前端（MSM381 → MIC1P/MIC1N）
 *   - ES_I2C_SDA/SCL 与 GT911 触摸共用 I2C 总线（GPIO7/8）
 *   - 数字录音：ES7210_SDOUT → GPIO48（ES8311 ASDOUT 未接 ESP32-P4）
 *
 * 本文件仅宏定义，不包含 ESP-IDF 头文件（与 display_config.h / sd_tf_config.h 一致）。
 */

#include "display_config.h"

#include "driver/i2c_types.h"

/* -------------------------------------------------------------------------- */
/* I2C（与 touch_config.h 同总线、同引脚）                                     */
/* -------------------------------------------------------------------------- */

/** 7-bit 地址；CE 经 R1(10k) 拉至 ADC_GND */
#define ES8311_I2C_ADDR             0x18

#define ES8311_I2C_SDA_GPIO         7
#define ES8311_I2C_SCL_GPIO         8

#define ES8311_I2C_PORT             I2C_NUM_1
#define ES8311_I2C_SCL_HZ           100000

/* -------------------------------------------------------------------------- */
/* I2S0 — 网络名 CODEC_I2S0_* / ES7210_SDOUT                                   */
/* -------------------------------------------------------------------------- */

#define ES8311_I2S_PORT             0   /* I2S_NUM_0 */

/** MCLK → CODEC_I2S0_MCLK */
#define ES8311_I2S_MCLK_GPIO        13

/** BCLK / SCLK → CODEC_I2S0_SCLK */
#define ES8311_I2S_BCLK_GPIO        12

/** LRCK / WS → CODEC_I2S0_LRCK */
#define ES8311_I2S_WS_GPIO          10

/** ES7210 SDOUT → P4 I2S RX（录音数据） */
#define ES8311_I2S_DIN_GPIO         48

/* -------------------------------------------------------------------------- */
/* 常用音频格式默认值（驱动初始化时可覆盖）                                     */
/* -------------------------------------------------------------------------- */

/** ES8311 支持约 16 kHz～64 kHz；语音常用 16 kHz */
#define ES8311_DEFAULT_SAMPLE_RATE_HZ   16000

#define ES8311_DEFAULT_BITS_PER_SAMPLE  16

/** 单声道 MSM381 → ES8311 MIC1 */
#define ES8311_DEFAULT_CHANNELS         1

/** 使用 MCLK 时典型为 sample_rate × 256 */
#define ES8311_MCLK_MULTIPLE            256

/* -------------------------------------------------------------------------- */
/* 采麦 FreeRTOS 任务（app_mic.c）                                             */
/* -------------------------------------------------------------------------- */

#define ES8311_MIC_TASK_NAME            "mic_cap"
#define ES8311_MIC_TASK_STACK_WORDS     4096
#define ES8311_MIC_TASK_PRIO            (tskIDLE_PRIORITY + 5)

/** 每次 read_pcm 请求的帧数（单声道） */
#define ES8311_PCM_FRAMES_PER_READ      256

/** ES7210 7-bit I2C 地址（若总线上无器件则跳过配置） */
#define ES7210_I2C_ADDR                 0x40

#endif
