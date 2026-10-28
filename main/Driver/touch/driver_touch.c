#include "driver_touch.h"

#include "display_config.h"
#include "touch_config.h"

#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_log.h"

static const char *TAG = "driver_touch";

static i2c_master_bus_handle_t s_i2c_bus;
static esp_lcd_panel_io_handle_t s_tp_io;
static esp_lcd_touch_handle_t s_tp;

esp_err_t driver_touch_init(void)
{
    ESP_LOGI(TAG, "GT911 I2C SDA=%d SCL=%d", TP_I2C_SDA_GPIO, TP_I2C_SCL_GPIO);

    const i2c_master_bus_config_t i2c_bus_cfg = {
        .i2c_port = TP_I2C_PORT,
        .sda_io_num = TP_I2C_SDA_GPIO,
        .scl_io_num = TP_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = {
            .enable_internal_pullup = true,
        },
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&i2c_bus_cfg, &s_i2c_bus), TAG, "i2c bus");

    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    tp_io_config.scl_speed_hz = TP_I2C_SCL_HZ;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(s_i2c_bus, &tp_io_config, &s_tp_io), TAG, "tp io");

    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = (gpio_num_t)TP_RST_GPIO,
        .int_gpio_num = (gpio_num_t)TP_INT_GPIO,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };
    ESP_RETURN_ON_ERROR(esp_lcd_touch_new_i2c_gt911(s_tp_io, &tp_cfg, &s_tp), TAG, "gt911");

    ESP_LOGI(TAG, "GT911 ready");
    return ESP_OK;
}

i2c_master_bus_handle_t driver_touch_get_i2c_bus(void)
{
    return s_i2c_bus;
}

bool driver_touch_read(uint16_t *x, uint16_t *y)
{
    if (!s_tp || !x || !y) {
        return false;
    }

    if (esp_lcd_touch_read_data(s_tp) != ESP_OK) {
        return false;
    }

    esp_lcd_touch_point_data_t point = {0};
    uint8_t point_num = 0;
    if (esp_lcd_touch_get_data(s_tp, &point, &point_num, 1) != ESP_OK || point_num == 0) {
        return false;
    }

    *x = point.x;
    *y = point.y;
    return true;
}
