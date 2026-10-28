#ifndef __NET_PHY_CONFIG_H__
#define __NET_PHY_CONFIG_H__

/**
 * JC1060P470C / JC-ESP32P4-M3 — 有线以太网（RMII 10/100）
 *
 * 原理图 JC1060P470C_I_W_Y-V1.0：
 *   - U3 ESP32-P4：片内 EMAC（RMII + SMI）
 *   - U4：IP101GR1（QFN-32），RMII 10/100 单口 PHY
 *   - RJ1 HR6213S：RJ45 + 网络变压器
 *   - X1：25 MHz 晶振（PHY 参考时钟）
 *
 * SMI 引脚与厂商 Demo（esp_brookesia_phone / sdkconfig）一致。
 * RMII 数据脚按原理图网络名与 GPIO 对应关系整理。
 *
 * 本文件仅宏定义，不包含 ESP-IDF 头文件（与 uart_config.h / es8311_config.h 一致）。
 * 驱动初始化时可选用 esp_eth_phy_new_ip101() 等，phy_addr / reset / mdc / mdio 使用下列宏。
 */

#include "display_config.h"

/* -------------------------------------------------------------------------- */
/* 外置 PHY（U4）                                                              */
/* -------------------------------------------------------------------------- */

/** 与 U4 IP101GR1 对应：组件 espressif/ip101 → esp_eth_phy_new_ip101() */
#define NET_PHY_DRIVER_IP101        1
#define NET_PHY_PART                "IP101GR1"

/** PHY 七位 MDIO 地址（原理图 PHY_AD0/AD3 上拉/下拉 strapping） */
#define NET_PHY_ADDR                1

/** PHY_RSTN → GPIO51，低有效 */
#define NET_PHY_RST_GPIO            51
#define NET_PHY_RST_ACTIVE_HIGH     0

/* -------------------------------------------------------------------------- */
/* SMI（MDC / MDIO）                                                           */
/* -------------------------------------------------------------------------- */

#define NET_PHY_MDC_GPIO            31
#define NET_PHY_MDIO_GPIO           52

/* -------------------------------------------------------------------------- */
/* RMII ↔ ESP32-P4 EMAC（GPIO 矩阵）                                           */
/* -------------------------------------------------------------------------- */

#define NET_PHY_RMII_TXD0_GPIO      34
#define NET_PHY_RMII_TXD1_GPIO      35
#define NET_PHY_RMII_RXD0_GPIO      29
#define NET_PHY_RMII_RXD1_GPIO      30
#define NET_PHY_RMII_CRS_DV_GPIO    28
#define NET_PHY_RMII_TX_EN_GPIO     49
#define NET_PHY_RMII_REF_CLK_GPIO   50

/* -------------------------------------------------------------------------- */
/* 链路 / 硬件说明（无 GPIO）                                                   */
/* -------------------------------------------------------------------------- */

/** 10/100 Mbps，非千兆 */
#define NET_PHY_SPEED_10_100M       1

/** PHY 侧参考时钟（原理图 X1） */
#define NET_PHY_XTAL_HZ             25000000

/** RJ45 位号（HR6213S，集成变压器） */
#define NET_PHY_RJ45_DESIGNATOR     "RJ1"

#endif
