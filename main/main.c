#include "app_init.h"
#include "app_tasks.h"

#include "esp_log.h"

static const char *TAG = "main";

void app_main(void)
{
    if (app_init() != ESP_OK) 
    {
        ESP_LOGE(TAG, "app_init failed, halted");
        return;
    }

    if (app_tasks_start() != ESP_OK) 
    {
        ESP_LOGE(TAG, "app_tasks_start failed");
        return;
    }
}
