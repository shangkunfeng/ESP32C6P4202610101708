#ifndef __UART1_CONFIG_H__
#define __UART1_CONFIG_H__

/**
 * UART1 引脚与默认串口参数（JC1060P470C / 扩展串口）
 *
 * 硬件映射：
 *   UART1 TXD → GPIO26
 *   UART1 RXD → GPIO27
 *
 * 与 UART0（uart_config.h / driver_uart）独立。
 * UART0 接收 CO2/温度/湿度/转速实时值；UART1 把这四路加上本机声音峰值送到 Serial Studio。
 */

#include "display_config.h"

#include "driver/uart.h"

#define UART1_PORT_NUM              UART_NUM_1

#define UART1_TX_GPIO               26
#define UART1_RX_GPIO               27

#define UART1_RTS_GPIO              PIN_NC
#define UART1_CTS_GPIO              PIN_NC

#define UART1_BAUD_RATE             115200
#define UART1_DATA_BITS             UART_DATA_8_BITS
#define UART1_PARITY                UART_PARITY_DISABLE
#define UART1_STOP_BITS             UART_STOP_BITS_1
#define UART1_FLOW_CTRL             UART_HW_FLOWCTRL_DISABLE

#define UART1_RX_BUF_SIZE           2048
#define UART1_TX_BUF_SIZE           2048
#define UART1_QUEUE_DEPTH           0

/* -------------------------------------------------------------------------- */
/* UART1 输出：Serial Studio Pro Quick Plot 与 PCM 流共用这条线，只能开一个   */
/* -------------------------------------------------------------------------- */

/**
 * Quick Plot 文本行（app_uart1_plot.c），逗号分隔、换行结束：
 *   CO2,温度,湿度,转速,声音\n
 * 例：800.00,25.30,60.12,1500.00,1480\n
 */
#define UART1_SERIAL_STUDIO_PLOT_ENABLE 1

/** Quick Plot 发送周期（毫秒） */
#define UART1_PLOT_INTERVAL_MS          100

/** 1：每帧 I2S 读完后向 UART1 发送 PCM；与 Quick Plot 互斥 */
#define UART1_MIC_STREAM_ENABLE         0

#if UART1_MIC_STREAM_ENABLE && UART1_SERIAL_STUDIO_PLOT_ENABLE
#error "UART1: enable either UART1_MIC_STREAM_ENABLE or UART1_SERIAL_STUDIO_PLOT_ENABLE, not both"
#endif

/**
 * 发送前抽点（16000 Hz / DECIM = 线上等效采样率）。
 * 115200 8N1 约 11.5 KB/s，DECIM=4 → 4 kHz × 2 字节 ≈ 8 KB/s。
 */
#define UART1_MIC_PCM_DECIM             4

/** 单次 write 最大字节（与 driver_uart1 TX 包上限一致） */
#define UART1_MIC_TX_CHUNK_BYTES        512

#endif
