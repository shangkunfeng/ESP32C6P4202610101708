#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * UART0（GPIO37 TX / GPIO38 RX）
 * - 收：硬件 RX 中断 → IDF 事件 → uart_rx 任务入环，driver_uart_read 信号量等待
 * - 发：driver_uart_write 入队 → uart_tx 任务 uart_write_bytes（底层 TX 中断填 FIFO）
 */
esp_err_t driver_uart_init(void);

esp_err_t driver_uart_deinit(void);

bool driver_uart_is_inited(void);

/** 将数据送入 TX 队列，由 uart_tx 任务发送；单包最大 512 字节 */
esp_err_t driver_uart_write(const uint8_t *data, size_t len, size_t *bytes_written);

/**
 * 从软件 RX 环读取；无数据时由 RX 中断路径 give 的信号量唤醒，最长等待 timeout_ticks。
 * timeout_ticks == 0 表示不等待。
 */
esp_err_t driver_uart_read(uint8_t *buf, size_t max_len, size_t *out_len, TickType_t timeout_ticks);

size_t driver_uart_rx_available(void);

void driver_uart_flush_rx(void);

/** 在 uart_rx 任务上下文调用（紧接 ISR 事件之后），宜只做轻量通知 */
typedef void (*driver_uart_rx_cb_t)(const uint8_t *data, size_t len, void *user_ctx);

esp_err_t driver_uart_set_rx_callback(driver_uart_rx_cb_t cb, void *user_ctx);

#ifdef __cplusplus
}
#endif
