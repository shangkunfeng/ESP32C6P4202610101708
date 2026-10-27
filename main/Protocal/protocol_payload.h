#pragma once

/**
 * 串口线上载荷（与 Driver 内 *_structCtrl_t 解耦，由协议层映射）。
 * 启停：en 为 0/1；浮点为小端 IEEE754（与设备一致）。
 */

#include <stdint.h>

#pragma pack(push, 1)

/** CO2：屏发启停 + CO2_Set_Value */
typedef struct {
    uint8_t en;
    uint8_t reserved[3];
    float set_value;
} co2_proto_set_t;

typedef struct {
    float real_value; /** CO2_Real_Value */
} co2_proto_real_t;

/** 温度：屏发启停 + Temp_Set_Value */
typedef struct {
    uint8_t en;
    uint8_t reserved[3];
    float set_value;
} temp_proto_set_t;

typedef struct {
    float real_value; /** Temp_Real_Value */
} temp_proto_real_t;

/** 湿度：屏发启停 + Hum_Set_Value */
typedef struct {
    uint8_t en;
    uint8_t reserved[3];
    float set_value;
} hum_proto_set_t;

typedef struct {
    float real_value; /** Hum_Real_Value */
} hum_proto_real_t;

/** 转速：屏发启停 + RPM_Set_Value */
typedef struct {
    uint8_t en;
    uint8_t reserved[3];
    float set_value;
} rpm_proto_set_t;

typedef struct {
    float real_value; /** RPM_Real_Value */
} rpm_proto_real_t;

#pragma pack(pop)
