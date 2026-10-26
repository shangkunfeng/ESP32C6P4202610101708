#include "board_display.h"

#include "display_config.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "board_display";

static uint32_t s_backlight_percent = 100;

static esp_err_t board_backlight_init_hw(void)
{
    const ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LCD_BL_LEDC_TIMER_NUM,
        .freq_hz = LCD_BL_LEDC_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer_cfg), TAG, "ledc timer");

    const ledc_channel_config_t ch_cfg = {
        .gpio_num = LCD_BL_PWM_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LCD_BL_LEDC_CH_NUM,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LCD_BL_LEDC_TIMER_NUM,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&ch_cfg), TAG, "ledc channel");

    return ESP_OK;
}

static void board_lcd_reset_levels(int *active, int *inactive)
{
    if (LCD_RST_ACTIVE_HIGH) {
        *active = 1;
        *inactive = 0;
    } else {
        *active = 0;
        *inactive = 1;
    }
}

static esp_err_t board_rst_gpio_init(void)
{
    int active = 0;
    int inactive = 0;
    board_lcd_reset_levels(&active, &inactive);

    const gpio_config_t io = {
        .pin_bit_mask = 1ULL << LCD_RST_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&io), TAG, "rst gpio");
    ESP_RETURN_ON_ERROR(gpio_set_level(LCD_RST_GPIO, inactive), TAG, "rst idle");

    return ESP_OK;
}

esp_err_t board_display_hw_init(void)
{
    ESP_RETURN_ON_ERROR(board_backlight_init_hw(), TAG, "backlight hw");
    ESP_RETURN_ON_ERROR(board_set_backlight(0), TAG, "backlight off");
    ESP_RETURN_ON_ERROR(board_rst_gpio_init(), TAG, "rst gpio");
    ESP_LOGI(TAG, "display hw init OK (%dx%d RGB565)", LCD_H_RES, LCD_V_RES);
    return ESP_OK;
}

esp_err_t board_set_backlight(uint32_t brightness_percent)
{
    if (brightness_percent < LCD_BL_MIN_PERCENT)
    {
        brightness_percent = LCD_BL_MIN_PERCENT;
    }
    
    if (brightness_percent > 100) 
    {
        brightness_percent = 100;
    }

    const uint32_t duty = (LCD_BL_LEDC_DUTY_MAX * brightness_percent) / 100;
    ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_BL_LEDC_CH_NUM, duty), TAG, "set duty");
    ESP_RETURN_ON_ERROR(ledc_update_duty(LEDC_LOW_SPEED_MODE, LCD_BL_LEDC_CH_NUM), TAG, "update duty");
    s_backlight_percent = brightness_percent;
    ESP_LOGI(TAG, "backlight %lu%%", (unsigned long)brightness_percent);

    return ESP_OK;
}

uint32_t board_get_backlight(void)
{
    return s_backlight_percent;
}

esp_err_t board_lcd_reset_pulse(void)
{
    int active = 0;
    int inactive = 0;
    board_lcd_reset_levels(&active, &inactive);

    ESP_RETURN_ON_ERROR(gpio_set_level(LCD_RST_GPIO, inactive), TAG, "rst pre");
    vTaskDelay(pdMS_TO_TICKS(5));
    ESP_RETURN_ON_ERROR(gpio_set_level(LCD_RST_GPIO, active), TAG, "rst assert");
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_RETURN_ON_ERROR(gpio_set_level(LCD_RST_GPIO, inactive), TAG, "rst release");
    vTaskDelay(pdMS_TO_TICKS(120));
    ESP_LOGI(TAG, "LCD reset pulse on GPIO%d", LCD_RST_GPIO);

    return ESP_OK;
}
