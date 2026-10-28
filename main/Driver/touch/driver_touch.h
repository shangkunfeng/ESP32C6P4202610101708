#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** GT911 + I2C，只初始化一次 */
esp_err_t driver_touch_init(void);

/** 与 ES8311 等器件共用 SDA/SCL 时复用该 bus（须 touch_init 之后） */
i2c_master_bus_handle_t driver_touch_get_i2c_bus(void);

/** 读单点；true=按下并写入 x/y */
bool driver_touch_read(uint16_t *x, uint16_t *y);

#ifdef __cplusplus
}
#endif
