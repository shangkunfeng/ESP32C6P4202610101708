#include "ui_trend_events.h"

#include "app_lvgl.h"
#include "ui_common.h"

#include "esp_log.h"

static const char *TAG = "ui_trend_evt";

#define TREND_REALTIME_PERIOD_MS  (UI_TREND_SAMPLE_PERIOD_SEC * 1000U)

static void trend_realtime_timer_cb(lv_timer_t *timer)
{
    ui_trend_t *ui = lv_timer_get_user_data(timer);
    if (!ui || !ui->screen) {
        return;
    }
    if (lv_screen_active() != ui->screen) {
        return;
    }

    ui_trend_realtime_tick(ui);
}

static void trend_btn_back_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ESP_LOGI(TAG, "back -> ui_home");
    ui_change_screen(UI_SCR_ui_home);
}

static void trend_btn_realtime_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_trend_t *ui = lv_event_get_user_data(e);
    if (!ui) {
        return;
    }
    ESP_LOGI(TAG, "mode: realtime");
    ui_trend_set_view_mode(ui, UI_TREND_VIEW_REALTIME);
}

static void trend_btn_history_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_trend_t *ui = lv_event_get_user_data(e);
    if (!ui) {
        return;
    }
    ESP_LOGI(TAG, "mode: history (stub)");
    ui_trend_set_view_mode(ui, UI_TREND_VIEW_HISTORY);
}

static void trend_btn_y_zoom_in_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_trend_t *ui = lv_event_get_user_data(e);
    if (ui) {
        ui_trend_y_zoom_in(ui);
    }
}

static void trend_btn_y_zoom_out_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_trend_t *ui = lv_event_get_user_data(e);
    if (ui) {
        ui_trend_y_zoom_out(ui);
    }
}

static void trend_btn_x_zoom_in_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_trend_t *ui = lv_event_get_user_data(e);
    if (ui) {
        ui_trend_x_zoom_in(ui);
    }
}

static void trend_btn_x_zoom_out_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_trend_t *ui = lv_event_get_user_data(e);
    if (ui) {
        ui_trend_x_zoom_out(ui);
    }
}

static bool trend_obj_is_chart_subtree(const ui_trend_t *ui, const lv_obj_t *obj)
{
    if (!ui || !ui->chart || !obj) {
        return false;
    }
    while (obj) {
        if (obj == ui->chart) {
            return true;
        }
        obj = lv_obj_get_parent(obj);
    }
    return false;
}

static void trend_dismiss_point_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_trend_t *ui = lv_event_get_user_data(e);
    if (!ui) {
        return;
    }
    if (trend_obj_is_chart_subtree(ui, lv_event_get_target(e))) {
        return;
    }
    ui_trend_clear_point_info(ui);
}

void ui_trend_bind_click_dismiss(lv_obj_t *obj, ui_trend_t *ui)
{
    if (!obj || !ui) {
        return;
    }
    lv_obj_set_clickable(obj, true);
    lv_obj_add_event_cb(obj, trend_dismiss_point_cb, LV_EVENT_CLICKED, ui);
}

static void trend_chart_value_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    ui_trend_t *ui = lv_event_get_user_data(e);
    if (!ui || !ui->chart) {
        return;
    }

    const uint32_t id = lv_chart_get_pressed_point(ui->chart);
    if (id == (uint32_t)LV_CHART_POINT_NONE || id >= ui->point_cnt) {
        return;
    }
    ui_trend_on_chart_point_selected(ui, id);
}

static void trend_channel_btn_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_trend_t *ui = lv_event_get_user_data(e);
    const intptr_t ch = (intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
    if (!ui || ch < 0 || ch >= UI_TREND_CHANNEL_COUNT) {
        return;
    }
    ui_trend_apply_channel(ui, (ui_trend_channel_t)ch);
}

void ui_trend_events_register(ui_trend_t *ui)
{
    if (!ui || !ui->screen) {
        return;
    }

    app_lvgl_bind_swipe(ui->screen, (int)UI_SCR_ui_trend);

    for (int i = 0; i < UI_TREND_CHANNEL_COUNT; i++) {
        if (ui->btn_channel[i]) {
            lv_obj_set_user_data(ui->btn_channel[i], (void *)(intptr_t)i);
            ui_trend_bind_click_dismiss(ui->btn_channel[i], ui);
            lv_obj_add_event_cb(ui->btn_channel[i], trend_channel_btn_cb, LV_EVENT_CLICKED, ui);
        }
    }

    if (ui->btn_back) {
        ui_trend_bind_click_dismiss(ui->btn_back, ui);
        lv_obj_add_event_cb(ui->btn_back, trend_btn_back_cb, LV_EVENT_CLICKED, NULL);
    }
    if (ui->btn_realtime) {
        ui_trend_bind_click_dismiss(ui->btn_realtime, ui);
        lv_obj_add_event_cb(ui->btn_realtime, trend_btn_realtime_cb, LV_EVENT_CLICKED, ui);
    }
    if (ui->btn_history) {
        ui_trend_bind_click_dismiss(ui->btn_history, ui);
        lv_obj_add_event_cb(ui->btn_history, trend_btn_history_cb, LV_EVENT_CLICKED, ui);
    }
    if (ui->btn_y_zoom_in) {
        ui_trend_bind_click_dismiss(ui->btn_y_zoom_in, ui);
        lv_obj_add_event_cb(ui->btn_y_zoom_in, trend_btn_y_zoom_in_cb, LV_EVENT_CLICKED, ui);
    }
    if (ui->btn_y_zoom_out) {
        ui_trend_bind_click_dismiss(ui->btn_y_zoom_out, ui);
        lv_obj_add_event_cb(ui->btn_y_zoom_out, trend_btn_y_zoom_out_cb, LV_EVENT_CLICKED, ui);
    }
    if (ui->btn_x_zoom_in) {
        ui_trend_bind_click_dismiss(ui->btn_x_zoom_in, ui);
        lv_obj_add_event_cb(ui->btn_x_zoom_in, trend_btn_x_zoom_in_cb, LV_EVENT_CLICKED, ui);
    }
    if (ui->btn_x_zoom_out) {
        ui_trend_bind_click_dismiss(ui->btn_x_zoom_out, ui);
        lv_obj_add_event_cb(ui->btn_x_zoom_out, trend_btn_x_zoom_out_cb, LV_EVENT_CLICKED, ui);
    }

    if (ui->chart) {
        lv_obj_add_event_cb(ui->chart, trend_chart_value_cb, LV_EVENT_VALUE_CHANGED, ui);
    }

    if (!ui->realtime_timer) {
        ui->realtime_timer =
            lv_timer_create(trend_realtime_timer_cb, TREND_REALTIME_PERIOD_MS, ui);
    }
}

void ui_trend_events_unregister(ui_trend_t *ui)
{
    if (!ui) {
        return;
    }
    if (ui->realtime_timer) {
        lv_timer_delete(ui->realtime_timer);
        ui->realtime_timer = NULL;
    }
}
