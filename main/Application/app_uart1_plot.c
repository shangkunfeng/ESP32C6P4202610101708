#include "app_uart1_plot.h"

#include "app_mic.h"
#include "driver_co2.h"
#include "driver_hum.h"
#include "driver_rpm.h"
#include "driver_temp.h"
#include "driver_uart1.h"
#include "uart1_config.h"

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>

static const char *TAG = "uart1_plot";

#if UART1_SERIAL_STUDIO_PLOT_ENABLE

static TaskHandle_t s_plot_task;
static bool s_plot_started;

static void uart1_plot_task(void *arg)
{
    (void)arg;

    char line[96];

    for (;;) 
    {
        if (driver_uart1_is_inited()) 
        {
            const float co2 = co2_struct.CO2_Real_Value;
            const float temp = temp_struct.Temp_Real_Value;
            const float hum = hum_struct.Hum_Real_Value;
            const float rpm = rpm_struct.RPM_Real_Value;
            const int sound = (int)app_mic_get_pcm_peak();

            const int n = snprintf(line, sizeof(line), "%.2f,%.2f,%.2f,%.2f,%d\n",
                                   (double)co2, (double)temp, (double)hum, (double)rpm, sound);
            if (n > 0 && (size_t)n < sizeof(line)) 
            {
                size_t sent = 0;
                (void)driver_uart1_write((const uint8_t *)line, (size_t)n, &sent);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(UART1_PLOT_INTERVAL_MS));
    }
}

esp_err_t app_uart1_plot_start(void)
{
    if (s_plot_started) {
        return ESP_OK;
    }
    ESP_RETURN_ON_FALSE(driver_uart1_is_inited(), ESP_ERR_INVALID_STATE, TAG, "uart1 not inited");

    BaseType_t ok = xTaskCreate(uart1_plot_task,
                                "uart1_plot",
                                3072,
                                NULL,
                                tskIDLE_PRIORITY + 2,
                                &s_plot_task);
    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_ERR_NO_MEM, TAG, "xTaskCreate");

    s_plot_started = true;
    ESP_LOGI(TAG,
             "Serial Studio Quick Plot on UART1: CO2,temp,hum,rpm,sound every %u ms",
             (unsigned)UART1_PLOT_INTERVAL_MS);
    return ESP_OK;
}

#else

esp_err_t app_uart1_plot_start(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

#endif
