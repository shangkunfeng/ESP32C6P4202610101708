#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 占位：CRC 计算（算法与范围待协议定稿） */
uint16_t uart_proto_crc16(const uint8_t *data, size_t len);

/** 占位：校验通过返回 true */
bool uart_proto_crc16_verify(const uint8_t *data, size_t len, uint16_t expect);

#ifdef __cplusplus
}
#endif
