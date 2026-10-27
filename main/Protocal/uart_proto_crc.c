#include "uart_proto_crc.h"

/** Modbus CRC16，多项式 0xA001，初值 0xFFFF */
uint16_t uart_proto_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    if (!data) {
        return crc;
    }
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

bool uart_proto_crc16_verify(const uint8_t *data, size_t len, uint16_t expect)
{
    return uart_proto_crc16(data, len) == expect;
}
