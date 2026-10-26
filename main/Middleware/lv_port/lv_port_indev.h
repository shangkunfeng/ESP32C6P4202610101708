#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * LVGL 9 触摸 port（GT911）。
 * 须：driver_touch_init() 且 lv_port_disp_init() 之后调用。
 */
esp_err_t lv_port_indev_init(void);

#ifdef __cplusplus
}
#endif
