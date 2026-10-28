#ifndef __UART_CONFIG_H__
#define __UART_CONFIG_H__

/**
 * UART0 引脚与默认串口参数（JC1060P470C / 用户排针）
 *
 * 硬件映射：
 *   UART0 TXD → GPIO37
 *   UART0 RXD → GPIO38
 *
 * 当前工程 printf / 日志仍走 USB Serial/JTAG（见根目录 sdkconfig.defaults 中
 * CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG）。本头文件供应用层 UART 驱动、Modbus、
 * 外设透传等使用；驱动初始化时请调用 uart_set_pin() 绑定下列 GPIO。
 *
 * 若要把 ROM 启动日志也迁到 UART0，需在 menuconfig：
 *   Component config → ESP System Settings → Channel for console output → UART
 *   并设置 TX/RX 为 37 / 38（或与下列宏一致的 CONFIG_ESP_CONSOLE_UART_*）。
 */

#include "display_config.h"

#include "driver/uart.h"

/* 控制器：UART0（ESP-IDF 中 UART_NUM_0） */
#define UART0_PORT_NUM              UART_NUM_0

#define UART0_TX_GPIO               37
#define UART0_RX_GPIO               38

/** 无硬件流控时保持 PIN_NC（见 display_config.h） */
#define UART0_RTS_GPIO              PIN_NC
#define UART0_CTS_GPIO              PIN_NC

/* 常用线参数（驱动层可覆盖） */
#define UART0_BAUD_RATE             115200
#define UART0_DATA_BITS             UART_DATA_8_BITS
#define UART0_PARITY                UART_PARITY_DISABLE
#define UART0_STOP_BITS             UART_STOP_BITS_1
#define UART0_FLOW_CTRL             UART_HW_FLOWCTRL_DISABLE

/* 驱动 install 默认缓冲（字节） */
#define UART0_RX_BUF_SIZE           1024
#define UART0_TX_BUF_SIZE           1024
#define UART0_QUEUE_DEPTH           0

#endif
