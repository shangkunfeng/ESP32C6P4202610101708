#pragma once

#include "esp_err.h"
#include "protocol_payload.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 屏→设备：解析 *_proto_set_t 写入对应 co2/temp/hum/rpm_struct */
esp_err_t uart_proto_payload_apply_set(uint8_t info_id, const uint8_t *payload, size_t len);

/** 设备→屏：从 struct 填充 *_proto_real_t 到 payload，len 入参为缓冲大小，出参为写入长度 */
esp_err_t uart_proto_payload_pack_real(uint8_t info_id, uint8_t *payload, size_t buf_len, size_t *out_len);

/** 屏→设备：组 *_proto_set_t（en + 当前设定值） */
esp_err_t uart_proto_payload_pack_set(uint8_t info_id, bool en, uint8_t *payload, size_t buf_len, size_t *out_len);

/** 设备→屏：解析 *_proto_real_t 写入 Real_Value（不改变 En） */
esp_err_t uart_proto_payload_apply_real(uint8_t info_id, const uint8_t *payload, size_t len);

#ifdef __cplusplus
}
#endif
