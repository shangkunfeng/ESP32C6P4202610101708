#pragma once

#include "esp_err.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UART_PROTO_FRAME_MAX_PAYLOAD    256

typedef struct {
    uint8_t info_id;   /** 通道：PROTO_INFO_ID_* */
    uint8_t info_cat;  /** 事件=设置下行；周期事件=实时上行（见 uart_proto_info_table） */
    uint16_t payload_len; /** 应与查表 expected_length 一致 */
    uint8_t payload[UART_PROTO_FRAME_MAX_PAYLOAD]; /** 见 protocol_payload.h 中 *_proto_set_t / *_proto_real_t */
} uart_proto_frame_t;

/** 向字节流喂入数据；成帧时 frame_ready=true 且 out 有效（可多次喂入） */
esp_err_t uart_proto_frame_feed(const uint8_t *chunk, size_t chunk_len, uart_proto_frame_t *out, bool *frame_ready);

void uart_proto_frame_parser_reset(void);

/** 占位：组帧到 buf，written 为实际长度 */
esp_err_t uart_proto_frame_build(const uart_proto_frame_t *frame, uint8_t *buf, size_t buf_size, size_t *written);

#ifdef __cplusplus
}
#endif
