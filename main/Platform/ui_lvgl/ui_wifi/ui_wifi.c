#include "ui_wifi.h"

#include "app_sd_background.h"
#include "app_wifi.h"
#include "display_config.h"
#include "sd_tf_config.h"
#include "ui_wifi_events.h"

#include "esp_log.h"

#include <stdio.h>
#include <stdint.h>

static const char *TAG = "ui_wifi";

LV_FONT_DECLARE(ui_font_home_22);
LV_FONT_DECLARE(lv_font_montserrat_14);
LV_FONT_DECLARE(lv_font_source_han_sans_sc_16_cjk);

/** SSID 可能是中文/全角等 UTF-8，不能用仅拉丁字的 Montserrat。 */
static const lv_font_t *wifi_ssid_font(void)
{
    return &lv_font_source_han_sans_sc_16_cjk;
}

static lv_image_dsc_t s_wifi_status_icon_dsc;

#define COL_CYAN   0x5EDFFF
#define COL_WHITE  0xF0F8FF
#define COL_MUTED  0xC8D8E8

static void style_screen(lv_obj_t *screen)
{
    lv_obj_set_scrollable(screen, false);
    lv_obj_set_size(screen, LCD_H_RES, LCD_V_RES);
    lv_obj_set_style_pad_all(screen, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(screen, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(screen, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_TRANSP, LV_PART_MAIN);
}

static void style_panel(lv_obj_t *panel)
{
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x0A1420), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x2A8FD4), LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(panel, 12, LV_PART_MAIN);
}

static void style_body_label(lv_obj_t *lbl)
{
    lv_obj_set_style_text_font(lbl, &ui_font_home_22, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_hex(COL_MUTED), LV_PART_MAIN);
}

static void style_status_label(lv_obj_t *lbl)
{
    lv_obj_set_style_text_font(lbl, &ui_font_home_22, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_hex(COL_CYAN), LV_PART_MAIN);
}

static void style_tool_btn(lv_obj_t *btn)
{
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1A2838), LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x3BCFFF), LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_80, LV_PART_MAIN);
}

static void style_primary_btn(lv_obj_t *btn)
{
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1E6FD9), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(btn, lv_color_hex(0x0E4FA8), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x5EDFFF), LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_80, LV_PART_MAIN);
}

static lv_obj_t *create_text_btn(lv_obj_t *parent, const char *text)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &ui_font_home_22, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_hex(COL_WHITE), LV_PART_MAIN);
    lv_obj_center(lbl);
    return btn;
}

static bool setup_wifi_icon_dsc(const uint16_t *px, lv_image_dsc_t *dsc)
{
    if (!px || !dsc) {
        return false;
    }
    dsc->header.magic = LV_IMAGE_HEADER_MAGIC;
    dsc->header.cf = LV_COLOR_FORMAT_RGB565;
    dsc->header.flags = 0;
    dsc->header.w = TF_WIFI_ICON_W;
    dsc->header.h = TF_WIFI_ICON_H;
    dsc->header.stride = (uint32_t)TF_WIFI_ICON_W * sizeof(uint16_t);
    dsc->data_size = TF_WIFI_ICON_BYTES;
    dsc->data = (const uint8_t *)px;
    return true;
}

static void style_ap_row(lv_obj_t *btn, bool selected)
{
    lv_obj_set_style_bg_color(btn, lv_color_hex(selected ? 0x1E6FD9 : 0x121C2C), LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, lv_color_hex(selected ? 0x5EDFFF : 0x2A4A68), LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(btn, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(btn, 8, LV_PART_MAIN);
}

void ui_wifi_refresh_status(ui_wifi_t *ui)
{
    if (!ui) {
        return;
    }

    char ip[20];
    char ssid[33];
    app_wifi_get_ip_str(ip, sizeof(ip));
    app_wifi_get_connected_ssid(ssid, sizeof(ssid));

    if (ui->lbl_conn_state) {
        if (app_wifi_is_connecting()) {
            lv_label_set_text(ui->lbl_conn_state, "连接状态：连接中");
        } else if (app_wifi_is_connected()) {
            lv_label_set_text(ui->lbl_conn_state, "连接状态：已连接");
        } else if (app_wifi_is_scanning()) {
            lv_label_set_text(ui->lbl_conn_state, "连接状态：扫描中");
        } else {
            lv_label_set_text(ui->lbl_conn_state, "连接状态：未连接");
        }
    }
    if (ui->lbl_ssid) {
        char line[64];
        if (ssid[0] != '\0') {
            snprintf(line, sizeof(line), "名称：%s", ssid);
        } else {
            snprintf(line, sizeof(line), "名称：—");
        }
        lv_label_set_text(ui->lbl_ssid, line);
    }
    if (ui->lbl_ip) {
        char line[32];
        snprintf(line, sizeof(line), "IP：%s", ip);
        lv_label_set_text(ui->lbl_ip, line);
    }

    const uint16_t *icon_px = app_wifi_is_connected()
                                  ? app_sd_background_get_wifi_link_rgb565()
                                  : app_sd_background_get_wifi_no_link_rgb565();
    if (!icon_px) {
        icon_px = app_sd_background_get_wifi_no_link_rgb565();
    }
    if (ui->img_status && icon_px && setup_wifi_icon_dsc(icon_px, &s_wifi_status_icon_dsc)) {
        lv_image_set_src(ui->img_status, &s_wifi_status_icon_dsc);
    }

    if (ui->lbl_list_title) {
        if (app_wifi_is_scanning()) {
            lv_label_set_text(ui->lbl_list_title, "附近无线网络（扫描中）");
        } else {
            lv_label_set_text(ui->lbl_list_title, "附近无线网络");
        }
    }
}

void ui_wifi_refresh_ap_list(ui_wifi_t *ui)
{
    if (!ui || !ui->list_ap) {
        return;
    }

    size_t total = app_wifi_scan_count();
    if (total > UI_WIFI_AP_LIST_MAX) {
        total = UI_WIFI_AP_LIST_MAX;
    }
    ui->ap_display_count = (int)total;

    if (ui->selected_ap >= ui->ap_display_count) {
        ui->selected_ap = ui->ap_display_count > 0 ? ui->ap_display_count - 1 : 0;
    }

    lv_obj_clean(ui->list_ap);
    for (int i = 0; i < ui->ap_display_count; i++) {
        const app_wifi_ap_record_t *ap = app_wifi_scan_get((size_t)i);
        if (!ap) {
            break;
        }

        lv_obj_t *row = lv_button_create(ui->list_ap);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, 44);
        style_ap_row(row, i == ui->selected_ap);

        lv_obj_t *name = lv_label_create(row);
        lv_label_set_text(name, ap->ssid);
        lv_obj_set_style_text_font(name, wifi_ssid_font(), LV_PART_MAIN);
        lv_obj_set_style_text_color(name, lv_color_hex(COL_WHITE), LV_PART_MAIN);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_set_width(name, lv_pct(62));
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);

        char rssi_buf[32];
        if (app_wifi_ap_needs_password(ap)) {
            snprintf(rssi_buf, sizeof(rssi_buf), "%d 加密", (int)ap->rssi);
        } else {
            snprintf(rssi_buf, sizeof(rssi_buf), "%d 开放", (int)ap->rssi);
        }
        lv_obj_t *rssi = lv_label_create(row);
        lv_label_set_text(rssi, rssi_buf);
        lv_obj_set_style_text_font(rssi, &ui_font_home_22, LV_PART_MAIN);
        lv_obj_set_style_text_color(rssi, lv_color_hex(COL_MUTED), LV_PART_MAIN);
        lv_obj_align(rssi, LV_ALIGN_RIGHT_MID, 0, 0);

        lv_obj_add_event_cb(row, ui_wifi_events_ap_row_cb, LV_EVENT_CLICKED, ui);
        lv_obj_set_user_data(row, (void *)(intptr_t)i);
    }
}

void ui_wifi_refresh(ui_wifi_t *ui)
{
    ui_wifi_refresh_status(ui);
    ui_wifi_refresh_ap_list(ui);
}

void ui_wifi_create(ui_wifi_t *ui, const lv_image_dsc_t *bg)
{
    if (!ui) {
        return;
    }

    ui->screen = NULL;
    ui->bg_img = NULL;
    ui->selected_ap = 0;
    ui->ap_display_count = 0;

    ui->screen = lv_obj_create(NULL);
    if (!ui->screen) {
        ESP_LOGE(TAG, "lv_obj_create(screen) failed");
        return;
    }
    style_screen(ui->screen);

    if (bg) {
        ui->bg_img = lv_image_create(ui->screen);
        lv_image_set_src(ui->bg_img, bg);
        lv_obj_set_size(ui->bg_img, LCD_H_RES, LCD_V_RES);
        lv_obj_align(ui->bg_img, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_set_clickable(ui->bg_img, false);
    }

    lv_obj_t *title_row = lv_obj_create(ui->screen);
    lv_obj_align(title_row, LV_ALIGN_TOP_MID, 0, 18);
    lv_obj_set_size(title_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(title_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(title_row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(title_row, 10, LV_PART_MAIN);
    lv_obj_set_flex_flow(title_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(title_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(title_row, false);

    ui->lbl_title = lv_label_create(title_row);
    lv_label_set_text(ui->lbl_title, "WiFi设置");
    style_status_label(ui->lbl_title);

    ui->img_status = lv_image_create(title_row);
    lv_obj_set_size(ui->img_status, TF_WIFI_ICON_W, TF_WIFI_ICON_H);

    ui->panel_status = lv_obj_create(ui->screen);
    lv_obj_set_size(ui->panel_status, LCD_H_RES - 80, 108);
    lv_obj_align(ui->panel_status, LV_ALIGN_TOP_MID, 0, 58);
    style_panel(ui->panel_status);
    lv_obj_set_style_pad_all(ui->panel_status, 16, LV_PART_MAIN);
    lv_obj_set_scrollable(ui->panel_status, false);

    ui->lbl_conn_state = lv_label_create(ui->panel_status);
    lv_label_set_text(ui->lbl_conn_state, "连接状态：未连接");
    style_status_label(ui->lbl_conn_state);
    lv_obj_align(ui->lbl_conn_state, LV_ALIGN_TOP_LEFT, 0, 0);

    ui->lbl_ssid = lv_label_create(ui->panel_status);
    lv_label_set_text(ui->lbl_ssid, "名称：—");
    style_body_label(ui->lbl_ssid);
    lv_obj_set_style_text_font(ui->lbl_ssid, wifi_ssid_font(), LV_PART_MAIN);
    lv_obj_align(ui->lbl_ssid, LV_ALIGN_TOP_LEFT, 0, 32);

    ui->lbl_ip = lv_label_create(ui->panel_status);
    lv_label_set_text(ui->lbl_ip, "IP：0.0.0.0");
    style_body_label(ui->lbl_ip);
    lv_obj_align(ui->lbl_ip, LV_ALIGN_TOP_LEFT, 0, 64);

    ui->panel_list = lv_obj_create(ui->screen);
    lv_obj_set_size(ui->panel_list, LCD_H_RES - 80, 318);
    lv_obj_align(ui->panel_list, LV_ALIGN_TOP_MID, 0, 178);
    style_panel(ui->panel_list);
    lv_obj_set_style_pad_all(ui->panel_list, 12, LV_PART_MAIN);
    lv_obj_set_flex_flow(ui->panel_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui->panel_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(ui->panel_list, 8, LV_PART_MAIN);

    ui->lbl_list_title = lv_label_create(ui->panel_list);
    lv_label_set_text(ui->lbl_list_title, "附近无线网络");
    style_status_label(ui->lbl_list_title);

    ui->list_ap = lv_obj_create(ui->panel_list);
    lv_obj_set_width(ui->list_ap, lv_pct(100));
    lv_obj_set_flex_grow(ui->list_ap, 1);
    lv_obj_set_style_bg_opa(ui->list_ap, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui->list_ap, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_row(ui->list_ap, 6, LV_PART_MAIN);
    lv_obj_set_flex_flow(ui->list_ap, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollable(ui->list_ap, true);
    lv_obj_set_scroll_dir(ui->list_ap, LV_DIR_VER);

    ui->panel_bar = lv_obj_create(ui->screen);
    lv_obj_set_size(ui->panel_bar, LCD_H_RES, 88);
    lv_obj_align(ui->panel_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(ui->panel_bar, lv_color_hex(0x0A1420), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui->panel_bar, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui->panel_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(ui->panel_bar, 8, LV_PART_MAIN);
    lv_obj_set_flex_flow(ui->panel_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui->panel_bar, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(ui->panel_bar, false);

    ui->btn_scan = create_text_btn(ui->panel_bar, "扫描");
    lv_obj_set_size(ui->btn_scan, 160, 56);
    style_tool_btn(ui->btn_scan);

    ui->btn_connect = create_text_btn(ui->panel_bar, "连接");
    lv_obj_set_size(ui->btn_connect, 160, 56);
    style_primary_btn(ui->btn_connect);

    ui->btn_back = create_text_btn(ui->panel_bar, "返回");
    lv_obj_set_size(ui->btn_back, 160, 56);
    style_tool_btn(ui->btn_back);

    ui->pwd_overlay = lv_obj_create(ui->screen);
    lv_obj_set_size(ui->pwd_overlay, LCD_H_RES, LCD_V_RES);
    lv_obj_align(ui->pwd_overlay, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(ui->pwd_overlay, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui->pwd_overlay, LV_OPA_60, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui->pwd_overlay, 0, LV_PART_MAIN);
    lv_obj_add_flag(ui->pwd_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui->pwd_overlay, LV_OBJ_FLAG_CLICKABLE);

    ui->pwd_panel = lv_obj_create(ui->pwd_overlay);
    lv_obj_set_size(ui->pwd_panel, 520, 300);
    lv_obj_align(ui->pwd_panel, LV_ALIGN_CENTER, 0, -40);
    style_panel(ui->pwd_panel);
    lv_obj_set_style_pad_all(ui->pwd_panel, 20, LV_PART_MAIN);

    ui->pwd_title = lv_label_create(ui->pwd_panel);
    lv_label_set_text(ui->pwd_title, "请输入WiFi密码");
    style_status_label(ui->pwd_title);
    lv_obj_align(ui->pwd_title, LV_ALIGN_TOP_LEFT, 0, 0);

    ui->pwd_ta = lv_textarea_create(ui->pwd_panel);
    lv_obj_set_size(ui->pwd_ta, lv_pct(100), 52);
    lv_obj_align(ui->pwd_ta, LV_ALIGN_TOP_MID, 0, 48);
    lv_textarea_set_one_line(ui->pwd_ta, true);
    lv_textarea_set_password_mode(ui->pwd_ta, true);
    lv_textarea_set_max_length(ui->pwd_ta, 63);
    lv_obj_set_style_radius(ui->pwd_ta, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui->pwd_ta, lv_color_hex(0xF5F8FC), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui->pwd_ta, wifi_ssid_font(), LV_PART_MAIN);

    lv_obj_t *pwd_row = lv_obj_create(ui->pwd_panel);
    lv_obj_set_size(pwd_row, lv_pct(100), 56);
    lv_obj_align(pwd_row, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(pwd_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(pwd_row, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(pwd_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pwd_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    ui->pwd_btn_cancel = create_text_btn(pwd_row, "取消");
    lv_obj_set_size(ui->pwd_btn_cancel, 200, 48);
    style_tool_btn(ui->pwd_btn_cancel);

    ui->pwd_btn_ok = create_text_btn(pwd_row, "确定");
    lv_obj_set_size(ui->pwd_btn_ok, 200, 48);
    style_primary_btn(ui->pwd_btn_ok);

    ui->pwd_keyboard = lv_keyboard_create(ui->pwd_overlay);
    lv_obj_set_size(ui->pwd_keyboard, LCD_H_RES, 200);
    lv_obj_align(ui->pwd_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_mode(ui->pwd_keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_obj_add_flag(ui->pwd_keyboard, LV_OBJ_FLAG_HIDDEN);

    lv_obj_move_foreground(title_row);
    lv_obj_move_foreground(ui->panel_status);
    lv_obj_move_foreground(ui->panel_list);
    lv_obj_move_foreground(ui->panel_bar);
    lv_obj_move_foreground(ui->pwd_overlay);

    ui_wifi_refresh(ui);
    ui_wifi_events_register(ui);

    ESP_LOGI(TAG, "ui_wifi layout created");
}

void ui_wifi_show(const ui_wifi_t *ui)
{
    if (!ui || !ui->screen) {
        return;
    }
    lv_screen_load(ui->screen);
}

void ui_wifi_on_screen_active(ui_wifi_t *ui)
{
    if (!ui) {
        return;
    }
    ui_wifi_refresh(ui);
    if (app_wifi_is_ready() && !app_wifi_is_scanning() && app_wifi_scan_count() == 0) {
        app_wifi_start_scan();
        ui_wifi_refresh_status(ui);
    }
}

void ui_wifi_on_screen_leave(ui_wifi_t *ui)
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

void ui_wifi_destroy(ui_wifi_t *ui)
{
    if (!ui) {
        return;
    }
    if (ui->screen) {
        lv_obj_delete(ui->screen);
        ui->screen = NULL;
        ui->bg_img = NULL;
    }
}
