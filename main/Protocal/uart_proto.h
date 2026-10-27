#pragma once

#include "esp_err.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 收到实时/设置帧并写入 struct 后回调（宜在回调里 lv_async_call 刷新 UI） */
typedef void (*uart_proto_ui_notify_fn)(void);

void uart_proto_set_ui_notify(uart_proto_ui_notify_fn fn);

esp_err_t uart_proto_init(void);

esp_err_t uart_proto_deinit(void);

/** 启动 UART 接收解析任务 */
esp_err_t uart_proto_start(void);

void uart_proto_stop(void);

esp_err_t uart_proto_send_set(uint8_t info_id, bool en);

esp_err_t uart_proto_send_all_enable(bool en);

#ifdef __cplusplus
}
#endif
