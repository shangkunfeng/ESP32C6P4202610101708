/**
 * LVGL 9 输入 port — GT911 via driver_touch_read
 *
 * driver_touch_init() → lv_port_disp_init() → lv_port_indev_init()
 */

#include "lv_port_indev.h"

#include "driver_touch.h"

#include "esp_check.h"
#include "esp_log.h"
#include "lvgl.h"

static const char *TAG = "lv_port_indev";

static lv_indev_t *s_indev;
static int16_t s_last_x;
static int16_t s_last_y;

static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;

    uint16_t x = 0;
    uint16_t y = 0;
    if (driver_touch_read(&x, &y)) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = (int16_t)x;
        data->point.y = (int16_t)y;
        s_last_x = data->point.x;
        s_last_y = data->point.y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
        data->point.x = s_last_x;
        data->point.y = s_last_y;
    }
}

esp_err_t lv_port_indev_init(void)
{
    ESP_RETURN_ON_FALSE(lv_display_get_default(), ESP_ERR_INVALID_STATE, TAG,
                        "call lv_port_disp_init first");

    s_indev = lv_indev_create();
    ESP_RETURN_ON_FALSE(s_indev, ESP_ERR_NO_MEM, TAG, "lv_indev_create");

    lv_indev_set_type(s_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(s_indev, touch_read_cb);
    lv_indev_set_display(s_indev, lv_display_get_default());

    ESP_LOGI(TAG, "pointer indev ready (GT911)");
    return ESP_OK;
}
