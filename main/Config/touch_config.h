#ifndef __TOUCH_CONFIG_H__
#define __TOUCH_CONFIG_H__

#include "display_config.h"

#include "driver/i2c_types.h"

/* GT911 I2C（JC1060P470C：SDA=GPIO7, SCL=GPIO8；RST/INT 未接） */
#define TP_I2C_SDA_GPIO           7
#define TP_I2C_SCL_GPIO           8
#define TP_RST_GPIO               PIN_NC
#define TP_INT_GPIO               PIN_NC

#define TP_I2C_PORT               I2C_NUM_1
#define TP_I2C_SCL_HZ             100000

/* 松手时水平位移超过该值才切图（像素，越小越灵敏；原 1/5 屏约 204，现 1/10 约 102） */
#define TP_SWIPE_COMMIT_PX        (LCD_H_RES / 10)

#endif
