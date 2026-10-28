#ifndef __APP_UART1_PLOT_H__
#define __APP_UART1_PLOT_H__

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 周期向 UART1 发送 Serial Studio Quick Plot 行（须 driver_uart1_init）。
 * 列顺序：CO2, 温度, 湿度, 转速, 声音峰值。
 */
esp_err_t app_uart1_plot_start(void);

#ifdef __cplusplus
}
#endif

#endif
