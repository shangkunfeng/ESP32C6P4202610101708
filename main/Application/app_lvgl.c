#include "app_lvgl.h"

#include "app_tasks.h"
#include "app_sd_background.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "touch_config.h"
#include "ui_common.h"

#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/task.h"
#include "lvgl.h"

#include <stdint.h>

static const char *TAG = "app_lvgl";

static esp_timer_handle_t s_lv_tick_timer;
static lv_point_t s_swipe_press;

static int wrap_screen(int i)
{
    while (i < 0) {
        i += UI_SCR_COUNT;
    }
    return i % UI_SCR_COUNT;
}

static void lv_tick_cb(void *arg)
{
    (void)arg;
    lv_lock();
    lv_tick_inc(2);
    lv_unlock();
}

static esp_err_t lv_tick_timer_start(void)
{
    const esp_timer_create_args_t args = {
        .callback = lv_tick_cb,
        .name = "lv_tick",
    };
    ESP_RETURN_ON_ERROR(esp_timer_create(&args, &s_lv_tick_timer), TAG, "tick timer create");
    ESP_RETURN_ON_ERROR(esp_timer_start_periodic(s_lv_tick_timer, 2000), TAG, "tick timer start");
    return ESP_OK;
}

void app_lvgl_show_index(int index)
{
    const int idx = wrap_screen(index);
    ui_change_screen((ui_screen_id_t)idx);
    if (idx == 0) {
        ESP_LOGI(TAG, "active screen %d (ui_login.bin)", idx);
    } else if (idx == 1) {
        ESP_LOGI(TAG, "active screen %d (ui_home_back.bin)", idx);
    } else if (idx == (int)UI_SCR_ui_param) {
        ESP_LOGI(TAG, "active screen %d (ui_param / ui_home.bin)", idx);
    } else {
        ESP_LOGI(TAG, "active screen %d (photo%d)", idx, idx % 3);
    }
}

static void swipe_event_cb(lv_event_t *e)
{
    const lv_event_code_t code = lv_event_get_code(e);
    const int screen_index = (int)(intptr_t)lv_event_get_user_data(e);
    lv_indev_t *indev = lv_indev_active();
    if (!indev) {
        return;
    }

    lv_point_t p;
    lv_indev_get_point(indev, &p);

    if (code == LV_EVENT_PRESSED) {
        s_swipe_press = p;
    } else if (code == LV_EVENT_RELEASED) {
        const int offset = (int)p.x - (int)s_swipe_press.x;
        if (offset >= TP_SWIPE_COMMIT_PX) {
            app_lvgl_show_index(screen_index - 1);
        } else if (offset <= -TP_SWIPE_COMMIT_PX) {
            app_lvgl_show_index(screen_index + 1);
        }
    }
}

void app_lvgl_bind_swipe(lv_obj_t *screen, int screen_index)
{
    if (!screen) {
        return;
    }

    void *ud = (void *)(intptr_t)wrap_screen(screen_index);
    lv_obj_add_event_cb(screen, swipe_event_cb, LV_EVENT_PRESSED, ud);
    lv_obj_add_event_cb(screen, swipe_event_cb, LV_EVENT_RELEASED, ud);
}

static void lvgl_task(void *arg)
{
    (void)arg;

    lv_init();
    ESP_ERROR_CHECK(lv_port_disp_init());
    ESP_ERROR_CHECK(lv_port_indev_init());
    ESP_ERROR_CHECK(lv_tick_timer_start());

    ui_init();

    ESP_LOGI(TAG, "LVGL task: ui_login + ui_home + ui_mode/lvgl_6 + trend + alarm + param, swipe switch");
    ESP_LOGI(TAG, "lvgl ready heap free=%u min_free=%u",
             (unsigned)esp_get_free_heap_size(),
             (unsigned)esp_get_minimum_free_heap_size());

    while (1) {
        const uint32_t delay_ms = lv_timer_handler();
        uint32_t wait = (delay_ms < 1) ? 1 : (delay_ms > 500 ? 500 : delay_ms);
        vTaskDelay(pdMS_TO_TICKS(wait));
    }
}

static BaseType_t lvgl_task_create(uint32_t stack_words)
{
    return xTaskCreate(lvgl_task,
                       APP_TASK_LVGL_NAME,
                       stack_words,
                       NULL,
                       APP_TASK_LVGL_PRIO,
                       NULL);
}

esp_err_t app_lvgl_start(void)
{
    ESP_RETURN_ON_FALSE(app_sd_background_is_ready(), ESP_ERR_INVALID_STATE, TAG,
                        "call app_sd_background_load_psram first");

    const uint32_t stack_words = APP_TASK_LVGL_STACK_WORDS;
    BaseType_t ok = lvgl_task_create(stack_words);

    if (ok != pdPASS) {
        ESP_LOGW(TAG,
                 "lvgl internal stack %u words failed, try PSRAM (int_free=%u spram_free=%u)",
                 (unsigned)stack_words,
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        ok = xTaskCreateWithCaps(lvgl_task,
                                 APP_TASK_LVGL_NAME,
                                 stack_words,
                                 NULL,
                                 APP_TASK_LVGL_PRIO,
                                 NULL,
                                 MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }

    if (ok != pdPASS) {
        ESP_LOGE(TAG,
                 "create lvgl task failed stack=%u words int_free=%u spram_free=%u",
                 (unsigned)stack_words,
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "lvgl task created stack=%u words (~%u KiB)",
             (unsigned)stack_words,
             (unsigned)((stack_words * (uint32_t)sizeof(StackType_t)) / 1024));
    return ESP_OK;
}
