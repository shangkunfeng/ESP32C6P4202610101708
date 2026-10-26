#include "ui_mode_events.h"

#include "app_lvgl.h"
#include "app_run_mode.h"
#include "ui_common.h"

#include "esp_log.h"

static const char *TAG = "ui_mode_evt";

static void mode_btn_auto_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_mode_t *ui = lv_event_get_user_data(e);
    app_run_mode_set(APP_RUN_MODE_AUTO);
    ESP_LOGI(TAG, "run mode: auto");
    ui_mode_refresh(ui);
    ui_home_sync_mode_label();
}

static void mode_btn_manual_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_mode_t *ui = lv_event_get_user_data(e);
    app_run_mode_set(APP_RUN_MODE_MANUAL);
    ESP_LOGI(TAG, "run mode: manual");
    ui_mode_refresh(ui);
    ui_home_sync_mode_label();
}

static void mode_btn_back_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_change_screen(UI_SCR_ui_home);
}

void ui_mode_events_register(ui_mode_t *ui)
{
    if (!ui || !ui->screen) {
        return;
    }

    app_lvgl_bind_swipe(ui->screen, (int)UI_SCR_ui_mode);

    if (ui->btn_auto) {
        lv_obj_add_event_cb(ui->btn_auto, mode_btn_auto_cb, LV_EVENT_CLICKED, ui);
    }
    if (ui->btn_manual) {
        lv_obj_add_event_cb(ui->btn_manual, mode_btn_manual_cb, LV_EVENT_CLICKED, ui);
    }
    if (ui->btn_back) {
        lv_obj_add_event_cb(ui->btn_back, mode_btn_back_cb, LV_EVENT_CLICKED, NULL);
    }
}
