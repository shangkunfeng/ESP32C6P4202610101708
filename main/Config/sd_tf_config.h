#pragma once

/**
 * JC1060P470C / JC-ESP32P4-M3 — TF Card（MicroSD）硬件参数
 *
 * 原理图页 TF_Card：
 *   - TF_VCC ← ESP_LDO_VO4，经 Q1(AO3401) 高侧开关
 *   - R10(NC)：GPIO45 未接栅极，R13(10k) 下拉 → Q1 默认导通（LDO 有电则槽有电）
 *   - 4-bit SDIO：SD_CLK/CMD/DATA0~3，带上拉（R2/R11/R12）
 *
 * SDMMC 使用 **控制器 slot 0**（driver_sd_tf 中 host.slot=0）。
 * P4 上 SDMMC_HOST_DEFAULT() 默认为 slot 1，与 ESP-Hosted/C6 SDIO 冲突，不可用于 TF。
 * 引脚与乐鑫 esp32_p4_function_ev_board BSP（Guition 1024×600 模组）一致；
 * 若与主原理图网络名不一致，以主原理图 / Demo 为准改 GPIO 宏。
 *
 * 本文件仅宏定义，不包含 ESP-IDF 头文件（与 display_config.h 风格一致）。
 */

#include "display_config.h"

/* -------------------------------------------------------------------------- */
/* 供电（内部 LDO，非 GPIO 直供）                                              */
/* -------------------------------------------------------------------------- */

/** 原理图网络 ESP_LDO_VO4 → esp_ldo_acquire_channel(chan_id) */
#define TF_LDO_PHY_CHAN             4

/** TF 卡 I/O 域典型 3.3V（以 eFuse / IDF LDO 配置为准，常用 3300 mV） */
#define TF_LDO_PHY_MV               3300

/* -------------------------------------------------------------------------- */
/* 可选：TF 电源 MOSFET 栅极（原理图 R10=NC 时软件可不使用）                    */
/* -------------------------------------------------------------------------- */

#define TF_PWR_GPIO                 45

/**
 * 1 = 原理图 R10 未焊，GPIO45 未控制 Q1，仅依赖 LDO + 默认导通
 * 0 = R10 已焊，需用 TF_PWR_GPIO 控制 Q1
 */
#define TF_PWR_GPIO_CTRL_NC         1

/* -------------------------------------------------------------------------- */
/* SDMMC / SDIO 4-bit（网络名 SD_*）                                           */
/* -------------------------------------------------------------------------- */

#define TF_SDMMC_CLK_GPIO           43
#define TF_SDMMC_CMD_GPIO           44
#define TF_SDMMC_D0_GPIO            39
#define TF_SDMMC_D1_GPIO            40
#define TF_SDMMC_D2_GPIO            41
#define TF_SDMMC_D3_GPIO            42

/** 4-bit 模式（与 DATA0~3 接法一致） */
#define TF_SDMMC_BUS_WIDTH          4

/** 卡检测：本板 4-bit 时 Pin2 作 DATA3，无独立 CD GPIO */
#define TF_CARD_DETECT_GPIO         PIN_NC

/* -------------------------------------------------------------------------- */
/* 后续挂载（Driver/Board 实现时使用，此处仅文档化默认值）                      */
/* -------------------------------------------------------------------------- */

/** VFS 挂载点 = SD 卡根目录（PC 上打开盘符后所见顶层） */
#define TF_VFS_MOUNT_POINT          "/sdcard"

/**
 * SD 卡根目录下第一层文件夹（PC：盘符\photo\ui_login.bin 等）
 * 须 sdkconfig 开启 FATFS LFN（见 sdkconfig.defaults）。
 */
#define TF_PHOTO_DIR_NAME           "photo"

/** 小图标（RGB565 原始像素，如 20×20） */
#define TF_IMAGES_DIR_NAME          "images"
#define TF_WIFI_ICON_W              20
#define TF_WIFI_ICON_H              20
#define TF_WIFI_ICON_BYTES          (TF_WIFI_ICON_W * TF_WIFI_ICON_H * 2u)
/** PSRAM/cache：分配与 msync 须按 128B 行对齐（800B 图标 → 896B） */
#define TF_WIFI_ICON_PSRAM_BYTES    (((TF_WIFI_ICON_BYTES) + 127u) / 128u * 128u)
#define TF_WIFI_NO_LINK_FILE        "wifi_no_link.bin"
#define TF_WIFI_LINK_FILE           "wifi_link.bin"

/** 登录界面全屏背景（photo 目录，RGB565 全屏 1024x600） */
#define TF_UI_LOGIN_FILE            "ui_login.bin"

/** 主页底图：仅静态装饰，动态数值由 LVGL 绘制（RGB565 全屏） */
#define TF_UI_HOME_BACK_FILE        "ui_home_back.bin"

/** 参数设置页全屏背景（SD photo 目录，RGB565 全屏） */
#define TF_UI_HOME_FILE             "ui_home.bin"

/** SDMMC 时钟上限（Hz）；读大文件可试 20M，不稳则改回 10M */
#define TF_SDMMC_FREQ_LIMIT_HZ      20000000

/** LDO 上电后等待 TF 槽稳定（ms） */
#define TF_LDO_POWERUP_DELAY_MS     200

/**
 * 1 = 挂载失败时自动分区并格式化为 FAT（会清空卡内数据，仅首次调试用）
 * 0 = 不格式化；卡须已为 FAT32/exFAT(FAT 兼容)，否则 mount 报 13 (FR_NO_FILESYSTEM)
 */
/** 1 会在 mount 失败时清空整卡；已有 FAT 和图片时请保持 0 */
#define TF_FORMAT_IF_MOUNT_FAILED   0

/* -------------------------------------------------------------------------- */
/* 用户参数（JSON，与 driver_* 结构体字段对应）                                 */
/* -------------------------------------------------------------------------- */

#define TF_PARAM_DIR_NAME           "param"
#define TF_PARAM_USER_FILE          "user_param.json"
#define TF_PARAM_USER_TMP           "user_param.tmp"

/* -------------------------------------------------------------------------- */
/* 历史趋势（historydata/co2|temp|humi|speed/YYYYMMDD.bin 或 latest.bin）      */
/* -------------------------------------------------------------------------- */

#define TF_HISTORY_DIR_NAME         "historydata"
#define TF_HISTORY_SUB_CO2          "co2"
#define TF_HISTORY_SUB_TEMP         "temp"
#define TF_HISTORY_SUB_HUMI         "humi"
#define TF_HISTORY_SUB_SPEED        "speed"
#define TF_HISTORY_FALLBACK_FILE    "latest.bin"
