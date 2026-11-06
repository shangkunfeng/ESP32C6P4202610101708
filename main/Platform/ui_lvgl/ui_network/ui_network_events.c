#include "ui_network_events.h"

#include "app_lvgl.h"
#include "app_network_url_probe.h"
#include "ui_common.h"

#include "esp_log.h"

static const char *TAG = "ui_network_evt";

static ui_network_t *s_ui;

static void network_keyboard_hide(ui_network_t *ui)
{
    if (!ui || !ui->keyboard) {
        return;
    }
    if (ui->ta_url) {
        lv_obj_remove_state(ui->ta_url, LV_STATE_FOCUSED);
    }
    lv_keyboard_set_textarea(ui->keyboard, NULL);
    lv_obj_set_hidden(ui->keyboard, true);
}

static void network_ta_event_cb(lv_event_t *e)
{
    ui_network_t *ui = lv_event_get_user_data(e);
    if (!ui || !ui->keyboard || !ui->ta_url) {
        return;
    }

    const lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_FOCUSED || code == LV_EVENT_CLICKED) {
        lv_keyboard_set_textarea(ui->keyboard, ui->ta_url);
        lv_obj_set_hidden(ui->keyboard, false);
        lv_obj_move_foreground(ui->keyboard);
        return;
    }
    if (code == LV_EVENT_DEFOCUSED || code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        network_keyboard_hide(ui);
    }
}

static void network_probe_done_cb(void *user_data,
                                  bool success,
                                  int http_status,
                                  const app_network_hmi_view_t *hmi,
                                  bool hmi_parsed)
{
    ui_network_t *ui = user_data;
    (void)http_status;
    if (!ui) {
        return;
    }
    ESP_LOGI(TAG, "probe done success=%d status=%d hmi=%d", success ? 1 : 0, http_status, hmi_parsed ? 1 : 0);
    ui_network_show_probe_result(ui, success);
    if (!success) {
        return;
    }
    if (hmi_parsed && hmi) {
        ui_network_show_hmi_panel(ui, hmi);
    } else {
        ui_network_show_hmi_parse_hint(ui);
    }
}

static void network_btn_login_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    ui_network_t *ui = lv_event_get_user_data(e);
    if (!ui || !ui->ta_url) {
        return;
    }

    network_keyboard_hide(ui);

    if (app_network_url_probe_busy()) {
        return;
    }

    const char *url = lv_textarea_get_text(ui->ta_url);
    if (!url || url[0] == '\0') {
        ui_network_show_probe_result(ui, false);
        return;
    }

    ui_network_set_probe_busy(ui, true);
    ui_network_clear_hmi_panel(ui);
    esp_err_t err = app_network_url_probe_start(url, network_probe_done_cb, ui);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "probe start: %s", esp_err_to_name(err));
        ui_network_show_probe_result(ui, false);
    }
}

static void network_btn_back_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    network_keyboard_hide(s_ui);
    ESP_LOGI(TAG, "back to ui_home");
    ui_change_screen(UI_SCR_ui_home);
}

void ui_network_events_on_screen_leave(ui_network_t *ui)
{
    network_keyboard_hide(ui);
    app_network_url_probe_cancel();
    ui_network_set_probe_busy(ui, false);
}

void ui_network_events_register(ui_network_t *ui)
{
    if (!ui) {
        return;
    }

    s_ui = ui;

    app_lvgl_bind_swipe(ui->screen, (int)UI_SCR_ui_network);

    if (ui->ta_url) {
        lv_obj_add_event_cb(ui->ta_url, network_ta_event_cb, LV_EVENT_ALL, ui);
    }

    if (ui->btn_back) {
        lv_obj_add_event_cb(ui->btn_back, network_btn_back_cb, LV_EVENT_CLICKED, NULL);
    }
    if (ui->btn_login) {
        lv_obj_add_event_cb(ui->btn_login, network_btn_login_cb, LV_EVENT_CLICKED, ui);
    }
}
