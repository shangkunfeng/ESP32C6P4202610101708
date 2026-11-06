#include "ui_network.h"

#include "app_eth.h"
#include "app_sd_background.h"
#include "display_config.h"
#include "sd_tf_config.h"
#include "ui_network_events.h"

#include "esp_log.h"

#include <stdio.h>

static const char *TAG = "ui_network";

LV_FONT_DECLARE(ui_font_home_22);

static lv_image_dsc_t s_network_icon_dsc;

#define COL_CYAN   0x5EDFFF
#define COL_WHITE  0xF0F8FF
#define COL_MUTED  0xC8D8E8
#define COL_OK     0x3DDC84
#define COL_FAIL   0xFF6B6B

static const char s_url_input_chars[] =
    "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
    ":/?#[]@!$&'()*+,;=%.-_~";

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

static void style_status_label(lv_obj_t *lbl)
{
    lv_obj_set_style_text_font(lbl, &ui_font_home_22, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_hex(COL_CYAN), LV_PART_MAIN);
}

static void style_body_label(lv_obj_t *lbl)
{
    lv_obj_set_style_text_font(lbl, &ui_font_home_22, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_hex(COL_MUTED), LV_PART_MAIN);
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

static void style_url_ta(lv_obj_t *ta)
{
    lv_obj_set_style_radius(ta, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ta, lv_color_hex(0xF5F8FC), LV_PART_MAIN);
    lv_obj_set_style_text_color(ta, lv_color_hex(0x1A2A3A), LV_PART_MAIN);
    lv_obj_set_style_text_font(ta, &ui_font_home_22, LV_PART_MAIN);
    lv_obj_set_style_pad_left(ta, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_right(ta, 12, LV_PART_MAIN);
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

static bool setup_network_icon_dsc(const uint16_t *px)
{
    if (!px) {
        return false;
    }
    s_network_icon_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    s_network_icon_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    s_network_icon_dsc.header.flags = 0;
    s_network_icon_dsc.header.w = TF_WIFI_ICON_W;
    s_network_icon_dsc.header.h = TF_WIFI_ICON_H;
    s_network_icon_dsc.header.stride = (uint32_t)TF_WIFI_ICON_W * sizeof(uint16_t);
    s_network_icon_dsc.data_size = TF_WIFI_ICON_BYTES;
    s_network_icon_dsc.data = (const uint8_t *)px;
    return true;
}

void ui_network_refresh_status(ui_network_t *ui)
{
    if (!ui) {
        return;
    }

    const bool link = app_eth_is_link_up();
    char ip[16] = "0.0.0.0";
    (void)app_eth_get_ip_str(ip, sizeof(ip));

    if (ui->lbl_link) {
        lv_label_set_text(ui->lbl_link, link ? "连接状态：已连接" : "连接状态：未连接");
    }
    if (ui->lbl_ip) {
        lv_label_set_text_fmt(ui->lbl_ip, "IP：%s", ip);
    }
}

void ui_network_set_probe_busy(ui_network_t *ui, bool busy)
{
    if (!ui || !ui->lbl_result) {
        return;
    }
    if (busy) {
        lv_label_set_text(ui->lbl_result, "正在连接...");
        lv_obj_set_style_text_color(ui->lbl_result, lv_color_hex(COL_MUTED), LV_PART_MAIN);
    }
    if (ui->btn_login) {
        if (busy) {
            lv_obj_add_state(ui->btn_login, LV_STATE_DISABLED);
        } else {
            lv_obj_remove_state(ui->btn_login, LV_STATE_DISABLED);
        }
    }
}

void ui_network_clear_hmi_panel(ui_network_t *ui)
{
    if (!ui || !ui->panel_hmi) {
        return;
    }
    if (ui->lbl_hmi_title) {
        lv_label_set_text(ui->lbl_hmi_title, "");
        lv_obj_set_hidden(ui->lbl_hmi_title, true);
    }
    if (ui->lbl_hmi_hint) {
        lv_label_set_text(ui->lbl_hmi_hint, "登录成功后显示服务器界面");
        lv_obj_set_style_text_color(ui->lbl_hmi_hint, lv_color_hex(COL_MUTED), LV_PART_MAIN);
        lv_obj_set_hidden(ui->lbl_hmi_hint, false);
    }
    for (size_t i = 0; i < APP_NETWORK_HMI_LINE_MAX; i++) {
        if (ui->lbl_hmi_lines[i]) {
            lv_label_set_text(ui->lbl_hmi_lines[i], "");
            lv_obj_set_hidden(ui->lbl_hmi_lines[i], true);
        }
    }
    for (size_t i = 0; i < APP_NETWORK_HMI_ITEM_MAX; i++) {
        if (ui->lbl_hmi_items[i]) {
            lv_label_set_text(ui->lbl_hmi_items[i], "");
            lv_obj_set_hidden(ui->lbl_hmi_items[i], true);
        }
    }
}

void ui_network_show_hmi_parse_hint(ui_network_t *ui)
{
    if (!ui) {
        return;
    }
    ui_network_clear_hmi_panel(ui);
    if (ui->lbl_hmi_hint) {
        lv_label_set_text(ui->lbl_hmi_hint, "登录成功，响应非 HMI JSON");
        lv_obj_set_style_text_color(ui->lbl_hmi_hint, lv_color_hex(COL_MUTED), LV_PART_MAIN);
    }
}

void ui_network_show_hmi_panel(ui_network_t *ui, const app_network_hmi_view_t *view)
{
    if (!ui || !view) {
        return;
    }

    ui_network_clear_hmi_panel(ui);
    if (ui->lbl_hmi_hint) {
        lv_obj_set_hidden(ui->lbl_hmi_hint, true);
    }

    int y = 0;
    if (view->title[0] != '\0' && ui->lbl_hmi_title) {
        lv_label_set_text(ui->lbl_hmi_title, view->title);
        style_status_label(ui->lbl_hmi_title);
        lv_obj_align(ui->lbl_hmi_title, LV_ALIGN_TOP_LEFT, 0, y);
        lv_obj_set_hidden(ui->lbl_hmi_title, false);
        y += 28;
    }

    for (size_t i = 0; i < view->line_count && i < APP_NETWORK_HMI_LINE_MAX; i++) {
        lv_obj_t *lbl = ui->lbl_hmi_lines[i];
        if (!lbl) {
            continue;
        }
        lv_label_set_text(lbl, view->lines[i]);
        style_body_label(lbl);
        lv_obj_set_width(lbl, LV_PCT(100));
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
        lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, y);
        lv_obj_set_hidden(lbl, false);
        y += 26;
    }

    for (size_t i = 0; i < view->item_count && i < APP_NETWORK_HMI_ITEM_MAX; i++) {
        lv_obj_t *lbl = ui->lbl_hmi_items[i];
        if (!lbl) {
            continue;
        }
        lv_label_set_text_fmt(lbl, "%s：%s", view->items[i].label, view->items[i].value);
        style_body_label(lbl);
        lv_obj_set_width(lbl, LV_PCT(100));
        lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, y);
        lv_obj_set_hidden(lbl, false);
        y += 26;
    }
}

void ui_network_show_probe_result(ui_network_t *ui, bool success)
{
    if (!ui || !ui->lbl_result) {
        return;
    }
    if (success) {
        lv_label_set_text(ui->lbl_result, "登录成功");
        lv_obj_set_style_text_color(ui->lbl_result, lv_color_hex(COL_OK), LV_PART_MAIN);
    } else {
        lv_label_set_text(ui->lbl_result, "登录失败");
        lv_obj_set_style_text_color(ui->lbl_result, lv_color_hex(COL_FAIL), LV_PART_MAIN);
        ui_network_clear_hmi_panel(ui);
    }
    ui_network_set_probe_busy(ui, false);
}

void ui_network_create(ui_network_t *ui, const lv_image_dsc_t *bg)
{
    if (!ui) {
        return;
    }

    ui->screen = lv_obj_create(NULL);
    style_screen(ui->screen);

    if (bg && bg->data) {
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
    lv_label_set_text(ui->lbl_title, "有线网络");
    style_status_label(ui->lbl_title);

    ui->img_status = lv_image_create(title_row);
    lv_obj_set_size(ui->img_status, TF_HOME_NETWORK_ICON_DISPLAY, TF_HOME_NETWORK_ICON_DISPLAY);
    const uint16_t *icon_px = app_sd_background_get_network_rgb565();
    if (icon_px && setup_network_icon_dsc(icon_px)) {
        lv_image_set_src(ui->img_status, &s_network_icon_dsc);
        lv_image_set_scale(ui->img_status,
                           (uint32_t)TF_HOME_NETWORK_ICON_DISPLAY * 256u / (uint32_t)TF_WIFI_ICON_W);
    }

    ui->panel_status = lv_obj_create(ui->screen);
    lv_obj_set_size(ui->panel_status, LCD_H_RES - 80, 88);
    lv_obj_align(ui->panel_status, LV_ALIGN_TOP_MID, 0, 58);
    style_panel(ui->panel_status);
    lv_obj_set_style_pad_all(ui->panel_status, 16, LV_PART_MAIN);
    lv_obj_set_scrollable(ui->panel_status, false);

    ui->lbl_link = lv_label_create(ui->panel_status);
    style_status_label(ui->lbl_link);
    lv_obj_align(ui->lbl_link, LV_ALIGN_TOP_LEFT, 0, 0);

    ui->lbl_ip = lv_label_create(ui->panel_status);
    style_body_label(ui->lbl_ip);
    lv_obj_align(ui->lbl_ip, LV_ALIGN_TOP_LEFT, 0, 36);

    ui->panel_url = lv_obj_create(ui->screen);
    lv_obj_set_size(ui->panel_url, LCD_H_RES - 80, 148);
    lv_obj_align(ui->panel_url, LV_ALIGN_TOP_MID, 0, 156);
    style_panel(ui->panel_url);
    lv_obj_set_style_pad_all(ui->panel_url, 16, LV_PART_MAIN);
    lv_obj_set_scrollable(ui->panel_url, false);

    ui->lbl_url_hint = lv_label_create(ui->panel_url);
    lv_label_set_text(ui->lbl_url_hint, "网址");
    style_status_label(ui->lbl_url_hint);
    lv_obj_align(ui->lbl_url_hint, LV_ALIGN_TOP_LEFT, 0, 0);

    ui->ta_url = lv_textarea_create(ui->panel_url);
    lv_obj_set_size(ui->ta_url, LV_PCT(100), 52);
    lv_obj_align(ui->ta_url, LV_ALIGN_TOP_LEFT, 0, 32);
    lv_textarea_set_one_line(ui->ta_url, true);
    lv_textarea_set_max_length(ui->ta_url, 127);
    lv_textarea_set_accepted_chars(ui->ta_url, s_url_input_chars);
    lv_textarea_set_placeholder_text(ui->ta_url, "http://www.example.com");
    lv_textarea_set_text(ui->ta_url, "http://www.baidu.com");
    style_url_ta(ui->ta_url);

    ui->lbl_result = lv_label_create(ui->panel_url);
    lv_label_set_text(ui->lbl_result, "");
    style_body_label(ui->lbl_result);
    lv_obj_align(ui->lbl_result, LV_ALIGN_TOP_LEFT, 0, 92);

    ui->panel_hmi = lv_obj_create(ui->screen);
    lv_obj_set_size(ui->panel_hmi, LCD_H_RES - 80, 200);
    lv_obj_align(ui->panel_hmi, LV_ALIGN_TOP_MID, 0, 308);
    style_panel(ui->panel_hmi);
    lv_obj_set_style_pad_all(ui->panel_hmi, 14, LV_PART_MAIN);
    lv_obj_set_scrollable(ui->panel_hmi, true);
    lv_obj_set_scroll_dir(ui->panel_hmi, LV_DIR_VER);

    ui->lbl_hmi_title = lv_label_create(ui->panel_hmi);
    lv_obj_set_width(ui->lbl_hmi_title, LV_PCT(100));
    lv_obj_set_hidden(ui->lbl_hmi_title, true);

    ui->lbl_hmi_hint = lv_label_create(ui->panel_hmi);
    lv_label_set_text(ui->lbl_hmi_hint, "登录成功后显示服务器界面");
    style_body_label(ui->lbl_hmi_hint);
    lv_obj_set_width(ui->lbl_hmi_hint, LV_PCT(100));
    lv_label_set_long_mode(ui->lbl_hmi_hint, LV_LABEL_LONG_WRAP);
    lv_obj_align(ui->lbl_hmi_hint, LV_ALIGN_TOP_LEFT, 0, 0);

    for (size_t i = 0; i < APP_NETWORK_HMI_LINE_MAX; i++) {
        ui->lbl_hmi_lines[i] = lv_label_create(ui->panel_hmi);
        lv_obj_set_hidden(ui->lbl_hmi_lines[i], true);
    }
    for (size_t i = 0; i < APP_NETWORK_HMI_ITEM_MAX; i++) {
        ui->lbl_hmi_items[i] = lv_label_create(ui->panel_hmi);
        lv_obj_set_hidden(ui->lbl_hmi_items[i], true);
    }

    ui->keyboard = lv_keyboard_create(ui->screen);
    lv_obj_set_size(ui->keyboard, LCD_H_RES, 200);
    lv_obj_align(ui->keyboard, LV_ALIGN_BOTTOM_MID, 0, -88);
    lv_keyboard_set_mode(ui->keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_obj_set_hidden(ui->keyboard, true);
    lv_obj_set_style_bg_color(ui->keyboard, lv_color_hex(0x142840), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui->keyboard, LV_OPA_COVER, LV_PART_MAIN);

    ui->panel_bar = lv_obj_create(ui->screen);
    lv_obj_set_size(ui->panel_bar, LCD_H_RES, 88);
    lv_obj_align(ui->panel_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(ui->panel_bar, lv_color_hex(0x0A1420), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui->panel_bar, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui->panel_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(ui->panel_bar, 8, LV_PART_MAIN);
    lv_obj_set_flex_flow(ui->panel_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui->panel_bar, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(ui->panel_bar, 24, LV_PART_MAIN);
    lv_obj_set_scrollable(ui->panel_bar, false);

    ui->btn_back = create_text_btn(ui->panel_bar, "返回");
    lv_obj_set_size(ui->btn_back, 160, 56);
    style_tool_btn(ui->btn_back);

    ui->btn_login = create_text_btn(ui->panel_bar, "登录");
    lv_obj_set_size(ui->btn_login, 160, 56);
    style_primary_btn(ui->btn_login);

    lv_obj_move_foreground(title_row);
    lv_obj_move_foreground(ui->panel_status);
    lv_obj_move_foreground(ui->panel_url);
    lv_obj_move_foreground(ui->panel_hmi);
    lv_obj_move_foreground(ui->panel_bar);
    lv_obj_move_foreground(ui->keyboard);

    ui_network_refresh_status(ui);
    ui_network_events_register(ui);

    ESP_LOGI(TAG, "ui_network layout created");
}

void ui_network_show(const ui_network_t *ui)
{
    if (ui && ui->screen) {
        lv_screen_load(ui->screen);
    }
}

void ui_network_on_screen_active(ui_network_t *ui)
{
    ui_network_refresh_status(ui);
}

void ui_network_on_screen_leave(ui_network_t *ui)
{
    ui_network_events_on_screen_leave(ui);
}

void ui_network_destroy(ui_network_t *ui)
{
    if (!ui) {
        return;
    }
    if (ui->screen) {
        lv_obj_delete(ui->screen);
        ui->screen = NULL;
    }
}
