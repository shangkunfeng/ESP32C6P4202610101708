#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * UART1（GPIO26 TX / GPIO27 RX）
 * - 收：硬件 RX 中断 → IDF 事件 → uart1_rx 任务入环，driver_uart1_read 信号量等待
 * - 发：driver_uart1_write 入队 → uart1_tx 任务 uart_write_bytes
 */
esp_err_t driver_uart1_init(void);

esp_err_t driver_uart1_deinit(void);

bool driver_uart1_is_inited(void);

esp_err_t driver_uart1_write(const uint8_t *data, size_t len, size_t *bytes_written);

esp_err_t driver_uart1_read(uint8_t *buf, size_t max_len, size_t *out_len, TickType_t timeout_ticks);

size_t driver_uart1_rx_available(void);

void driver_uart1_flush_rx(void);

typedef void (*driver_uart1_rx_cb_t)(const uint8_t *data, size_t len, void *user_ctx);

esp_err_t driver_uart1_set_rx_callback(driver_uart1_rx_cb_t cb, void *user_ctx);

#ifdef __cplusplus
}
#endif
