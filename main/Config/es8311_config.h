#ifndef __ES8311_CONFIG_H__
#define __ES8311_CONFIG_H__

/**
 * JC1060P470C / JC-ESP32P4-M3 — ES8311 音频 Codec 硬件参数
 *
 * 原理图 JC1060P470C_I_W_Y-V1.0（ES8311 页）：
 *   - U1 ES8311-S：I2C 配置 + I2S0 数字音频
 *   - ES_I2C_SDA/SCL 与 GT911 触摸共用 I2C 总线（GPIO7/8）
 *   - DAC_OUT_P/N → U2 NS4150 功放 → 喇叭；PA_CTRL 高电平使能功放
 *   - 板载 MSM381 模拟麦接 MIC1P/MIC1N；ES8311 ASDOUT 未接 ESP32-P4
 *     （多麦 ADC 走 ES7210，SDOUT → GPIO48，见独立配置）
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
/* I2S0 — 网络名 CODEC_I2S0_*                                                  */
/* -------------------------------------------------------------------------- */

#define ES8311_I2S_PORT             0   /* I2S_NUM_0 */

/** MCLK → CODEC_I2S0_MCLK */
#define ES8311_I2S_MCLK_GPIO        13

/** BCLK / SCLK → CODEC_I2S0_SCLK */
#define ES8311_I2S_BCLK_GPIO        12

/** LRCK / WS → CODEC_I2S0_LRCK */
#define ES8311_I2S_WS_GPIO          10

/** ESP TX → ES8311 DSDIN（播放） */
#define ES8311_I2S_DOUT_GPIO        9

/** ES8311 ASDOUT → ESP RX：本版原理图未连接 */
#define ES8311_I2S_DIN_GPIO         PIN_NC

/* -------------------------------------------------------------------------- */
/* 功放 NS4150（U2）                                                           */
/* -------------------------------------------------------------------------- */

/** 网络 PA_CTRL；高电平使能喇叭输出 */
#define ES8311_PA_CTRL_GPIO         11
#define ES8311_PA_CTRL_ACTIVE_HIGH  1

/* -------------------------------------------------------------------------- */
/* 常用音频格式默认值（驱动初始化时可覆盖）                                     */
/* -------------------------------------------------------------------------- */

/** ES8311 支持约 16 kHz～64 kHz；语音常用 16 kHz */
#define ES8311_DEFAULT_SAMPLE_RATE_HZ   16000

#define ES8311_DEFAULT_BITS_PER_SAMPLE  16

/** 单声道 MSM381 → ES8311 MIC1；播放为立体声 I2S 时由驱动填右声道 */
#define ES8311_DEFAULT_CHANNELS         1

/** 使用 MCLK 时典型为 sample_rate × 256 */
#define ES8311_MCLK_MULTIPLE            256

#endif
