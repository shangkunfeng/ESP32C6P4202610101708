#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UART_PROTO_FLOW_HOST_TO_DEVICE = 0, /** 屏/上位机 → 设备（设置） */
    UART_PROTO_FLOW_DEVICE_TO_HOST = 1, /** 设备 → 屏（实时值等） */
} uart_proto_flow_t;

typedef struct {
    uint8_t info_id;        /** PROTO_INFO_ID_CO2 / TEMP / HUM / RPM */
    uint8_t info_cat;       /** 事件=设置；周期事件=实时上报 */
    uart_proto_flow_t flow;   /** 数据流向 */
    size_t expected_length;   /** sizeof(对应 protocol_payload.h 中 proto 结构) */
    uint32_t period_ms;       /** 周期事件上报间隔；事件类为 0 */
} uart_proto_info_entry_t;

const uart_proto_info_entry_t *uart_proto_info_table_find(uint8_t info_id, uint8_t info_cat);

size_t uart_proto_info_table_count(void);

#ifdef __cplusplus
}
#endif
