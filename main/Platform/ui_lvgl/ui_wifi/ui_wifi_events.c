#include "ui_wifi_events.h"

#include "app_lvgl.h"
#include "app_wifi.h"
#include "ui_common.h"

#include "esp_log.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "ui_wifi_evt";

static ui_wifi_t *s_ui;

static void wifi_pwd_hide(ui_wifi_t *ui)
{
    if (!ui) {
        return;
    }
    if (ui->pwd_keyboard) {
        lv_keyboard_set_textarea(ui->pwd_keyboard, NULL);
        lv_obj_add_flag(ui->pwd_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
    if (ui->pwd_overlay) {
        lv_obj_add_flag(ui->pwd_overlay, LV_OBJ_FLAG_HIDDEN);
    }
}

static void wifi_pwd_show(ui_wifi_t *ui, const char *ssid)
{
    if (!ui || !ui->pwd_overlay || !ui->pwd_ta || !ui->pwd_title) {
        return;
    }
    lv_label_set_text(ui->pwd_title, "请输入WiFi密码");
    lv_textarea_set_text(ui->pwd_ta, "");
    if (ssid && ssid[0] != '\0') {
        lv_textarea_set_placeholder_text(ui->pwd_ta, ssid);
    }
    lv_obj_clear_flag(ui->pwd_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ui->pwd_overlay);
    if (ui->pwd_keyboard) {
        lv_obj_clear_flag(ui->pwd_keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_keyboard_set_textarea(ui->pwd_keyboard, ui->pwd_ta);
        lv_obj_move_foreground(ui->pwd_keyboard);
    }
}

static void wifi_begin_connect(ui_wifi_t *ui, const char *ssid, const char *password)
{
    if (!ssid || ssid[0] == '\0') {
        return;
    }
    esp_err_t err = app_wifi_connect(ssid, password ? password : "");
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "connect failed: %s", esp_err_to_name(err));
    }
    if (ui) {
        ui_wifi_refresh_status(ui);
    }
}

static const app_wifi_ap_record_t *wifi_selected_ap(ui_wifi_t *ui)
{
    if (!ui || ui->ap_display_count <= 0) {
        return NULL;
    }
    if (ui->selected_ap < 0 || ui->selected_ap >= ui->ap_display_count) {
        return NULL;
    }
    return app_wifi_scan_get((size_t)ui->selected_ap);
}

void ui_wifi_events_ap_row_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_wifi_t *ui = lv_event_get_user_data(e);
    lv_obj_t *row = lv_event_get_target(e);
    if (!ui || !row) {
        return;
    }
    ui->selected_ap = (int)(intptr_t)lv_obj_get_user_data(row);
    if (ui->selected_ap < 0) {
        ui->selected_ap = 0;
    }
    if (ui->selected_ap >= ui->ap_display_count) {
        ui->selected_ap = ui->ap_display_count > 0 ? ui->ap_display_count - 1 : 0;
    }
    ui_wifi_refresh_ap_list(ui);
}

static void wifi_btn_scan_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_wifi_t *ui = lv_event_get_user_data(e);
    if (!app_wifi_is_ready()) {
        ESP_LOGW(TAG, "Wi-Fi not ready");
        return;
    }
    if (app_wifi_is_scanning()) {
        return;
    }
    esp_err_t err = app_wifi_start_scan();
    ESP_LOGI(TAG, "scan start: %s", esp_err_to_name(err));
    if (ui) {
        ui_wifi_refresh_status(ui);
    }
}

static void wifi_btn_connect_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_wifi_t *ui = lv_event_get_user_data(e);
    const app_wifi_ap_record_t *ap = wifi_selected_ap(ui);
    if (!ap) {
        ESP_LOGW(TAG, "no AP selected");
        return;
    }
    if (app_wifi_ap_needs_password(ap)) {
        wifi_pwd_show(ui, ap->ssid);
        return;
    }
    wifi_begin_connect(ui, ap->ssid, "");
}

static void wifi_pwd_ok_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_wifi_t *ui = lv_event_get_user_data(e);
    const app_wifi_ap_record_t *ap = wifi_selected_ap(ui);
    if (!ap || !ui || !ui->pwd_ta) {
        return;
    }
    const char *pass = lv_textarea_get_text(ui->pwd_ta);
    wifi_pwd_hide(ui);
    wifi_begin_connect(ui, ap->ssid, pass);
}

static void wifi_pwd_cancel_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_wifi_t *ui = lv_event_get_user_data(e);
    wifi_pwd_hide(ui);
}

static void wifi_pwd_ta_focus_cb(lv_event_t *e)
{
    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_FOCUSED && code != LV_EVENT_CLICKED) {
        return;
    }
    ui_wifi_t *ui = lv_event_get_user_data(e);
    if (!ui || !ui->pwd_keyboard || !ui->pwd_ta) {
        return;
    }
    lv_obj_clear_flag(ui->pwd_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_keyboard_set_textarea(ui->pwd_keyboard, ui->pwd_ta);
    lv_obj_move_foreground(ui->pwd_keyboard);
}

static void wifi_btn_back_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (s_ui) {
        wifi_pwd_hide(s_ui);
    }
    ui_change_screen(UI_SCR_ui_home);
}

void ui_wifi_events_register(ui_wifi_t *ui)
{
    if (!ui || !ui->screen) {
        return;
    }

    s_ui = ui;

    app_lvgl_bind_swipe(ui->screen, (int)UI_SCR_ui_wifi);

    if (ui->btn_scan) {
        lv_obj_add_event_cb(ui->btn_scan, wifi_btn_scan_cb, LV_EVENT_CLICKED, ui);
    }
    if (ui->btn_connect) {
        lv_obj_add_event_cb(ui->btn_connect, wifi_btn_connect_cb, LV_EVENT_CLICKED, ui);
    }
    if (ui->btn_back) {
        lv_obj_add_event_cb(ui->btn_back, wifi_btn_back_cb, LV_EVENT_CLICKED, NULL);
    }
    if (ui->pwd_btn_ok) {
        lv_obj_add_event_cb(ui->pwd_btn_ok, wifi_pwd_ok_cb, LV_EVENT_CLICKED, ui);
    }
    if (ui->pwd_btn_cancel) {
        lv_obj_add_event_cb(ui->pwd_btn_cancel, wifi_pwd_cancel_cb, LV_EVENT_CLICKED, ui);
    }
    if (ui->pwd_ta) {
        lv_obj_add_event_cb(ui->pwd_ta, wifi_pwd_ta_focus_cb, LV_EVENT_FOCUSED, ui);
        lv_obj_add_event_cb(ui->pwd_ta, wifi_pwd_ta_focus_cb, LV_EVENT_CLICKED, ui);
    }
}
