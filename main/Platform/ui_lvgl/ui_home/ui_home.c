#include "ui_home.h"

#include "ui_home_events.h"

#include "app_run_mode.h"
#include "app_sd_background.h"
#include "app_wifi.h"
#include "display_config.h"
#include "sd_tf_config.h"
#include "esp_log.h"

#include <stdio.h>

static const char *TAG = "ui_home";

LV_FONT_DECLARE(ui_font_home_16);
LV_FONT_DECLARE(ui_font_home_22);

static lv_image_dsc_t s_home_wifi_no_link_dsc;
static lv_image_dsc_t s_home_wifi_link_dsc;

static bool home_wifi_fill_icon_dsc(lv_image_dsc_t *dsc, const uint16_t *px)
{
    if (!dsc || !px) {
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

void ui_home_refresh_wifi_icon(ui_home_t *ui)
{
    if (!ui || !ui->img_wifi) {
        return;
    }

    const uint16_t *px = NULL;
    lv_image_dsc_t *dsc = NULL;

    if (app_wifi_is_connected()) {
        px = app_sd_background_get_wifi_link_rgb565();
        if (px) {
            dsc = &s_home_wifi_link_dsc;
        }
    }
    if (!px) {
        px = app_sd_background_get_wifi_no_link_rgb565();
        dsc = &s_home_wifi_no_link_dsc;
    }
    if (!px || !dsc || !home_wifi_fill_icon_dsc(dsc, px)) {
        return;
    }
    lv_image_set_src(ui->img_wifi, dsc);
}
LV_FONT_DECLARE(lv_font_montserrat_28);
LV_FONT_DECLARE(lv_font_montserrat_14);

#define COL_CYAN        0x5EDFFF
#define COL_WHITE       0xF0F8FF
#define COL_GREEN       0x3DDC84
#define COL_ORANGE      0xFFB74D
#define COL_ARC_IND     0x00E5FF

static const lv_font_t *home_font_cn(void)
{
    return &ui_font_home_16;
}

static const lv_font_t *home_font_num(void)
{
    return &lv_font_montserrat_28;
}

static void style_screen(lv_obj_t *screen)
{
    lv_obj_set_scrollable(screen, false);
    lv_obj_set_size(screen, LCD_H_RES, LCD_V_RES);
    lv_obj_set_style_pad_all(screen, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(screen, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(screen, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_TRANSP, LV_PART_MAIN);
}

static void style_value_white(lv_obj_t *label)
{
    lv_obj_set_style_text_color(label, lv_color_hex(COL_WHITE), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, home_font_num(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, LV_PART_MAIN);
}

static void style_value_cyan(lv_obj_t *label)
{
    lv_obj_set_style_text_color(label, lv_color_hex(COL_CYAN), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, home_font_cn(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, LV_PART_MAIN);
}

static void style_status_green(lv_obj_t *label)
{
    lv_obj_set_style_text_color(label, lv_color_hex(COL_GREEN), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, home_font_cn(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, LV_PART_MAIN);
}

static void style_status_orange(lv_obj_t *label)
{
    lv_obj_set_style_text_color(label, lv_color_hex(COL_ORANGE), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, home_font_cn(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(label, LV_OPA_TRANSP, LV_PART_MAIN);
}

static void style_hit_area(lv_obj_t *obj)
{
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_20, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x3BCFFF), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(obj, 0, LV_PART_MAIN);
}

static void setup_gauge_arc(lv_obj_t *arc, int32_t min, int32_t max, int32_t value)
{
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_set_clickable(arc, false);
    lv_arc_set_mode(arc, LV_ARC_MODE_NORMAL);
    lv_arc_set_range(arc, min, max);
    lv_arc_set_value(arc, value);
    lv_arc_set_bg_angles(arc, 135, 45);
    lv_obj_set_style_arc_width(arc, 0, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 0, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
}

static lv_obj_t *create_mini_gauge(lv_obj_t *parent, int32_t x, int32_t y, int32_t size,
                                   const char *value, int32_t arc_min, int32_t arc_max, int32_t arc_val,
                                   lv_obj_t **arc_out, lv_obj_t **val_out)
{
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_set_pos(box, x, y);
    lv_obj_set_size(box, size, size);
    lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(box, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(box, 0, LV_PART_MAIN);
    lv_obj_set_scrollable(box, false);

    lv_obj_t *arc = lv_arc_create(box);
    lv_obj_set_size(arc, size, size);
    lv_obj_center(arc);
    setup_gauge_arc(arc, arc_min, arc_max, arc_val);

    lv_obj_t *val_lbl = lv_label_create(box);
    lv_label_set_text(val_lbl, value);
    style_value_white(val_lbl);
    lv_obj_align(val_lbl, LV_ALIGN_CENTER, 0, 8);

    if (arc_out) {
        *arc_out = arc;
    }
    if (val_out) {
        *val_out = val_lbl;
    }
    return box;
}

static void style_title_row(lv_obj_t *row)
{
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(row, 8, LV_PART_MAIN);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(row, false);
}

static void create_header_overlay(lv_obj_t *screen, ui_home_t *ui)
{
    ui->img_wifi = NULL;

    lv_obj_t *title_row = lv_obj_create(screen);
    lv_obj_align(title_row, LV_ALIGN_TOP_MID, -155, 14);
    style_title_row(title_row);
    lv_obj_set_size(title_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

    /* 与底图「工控主监控」同字体占位，WiFi 图标紧挨其右侧（文字本身透明，不叠字） */
    lv_obj_t *lbl_title = lv_label_create(title_row);
    lv_label_set_text(lbl_title, "工控主监控");
    lv_obj_set_style_text_font(lbl_title, &ui_font_home_22, LV_PART_MAIN);
    lv_obj_set_style_text_opa(lbl_title, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lbl_title, LV_OPA_TRANSP, LV_PART_MAIN);

    if (app_sd_background_get_wifi_no_link_rgb565()) {
        ui->img_wifi = lv_image_create(title_row);
        lv_obj_set_size(ui->img_wifi, TF_WIFI_ICON_W, TF_WIFI_ICON_H);
        lv_obj_set_clickable(ui->img_wifi, true);
        lv_obj_set_ext_click_area(ui->img_wifi, 8);
        ui_home_refresh_wifi_icon(ui);
    }

    ui->lbl_time = lv_label_create(screen);
    lv_label_set_text(ui->lbl_time, "时间：10:28");
    style_value_cyan(ui->lbl_time);
    lv_obj_align(ui->lbl_time, LV_ALIGN_TOP_RIGHT, -24, 18);

    ui->lbl_mode = lv_label_create(screen);
    lv_label_set_text(ui->lbl_mode, app_run_mode_line_text());
    style_value_cyan(ui->lbl_mode);
    lv_obj_align(ui->lbl_mode, LV_ALIGN_TOP_LEFT, 24, 18);
}

static void create_status_label(lv_obj_t *parent, int32_t x, int32_t y, int32_t w,
                                const char *status, lv_obj_t **out)
{
    lv_obj_t *st = lv_label_create(parent);
    lv_label_set_text(st, status);
    lv_obj_set_style_text_font(st, home_font_cn(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(st, LV_OPA_TRANSP, LV_PART_MAIN);

    if ((strcmp(status, "运行") == 0) || (strcmp(status, "开") == 0)) {
        lv_obj_set_style_text_color(st, lv_color_hex(0x00c853), LV_PART_MAIN);
    } else if (strcmp(status, "故障") == 0) {
        lv_obj_set_style_text_color(st, lv_color_hex(0xff0000), LV_PART_MAIN);
    } else if (strcmp(status, "关") == 0) {
        lv_obj_set_style_text_color(st, lv_color_hex(0xffab00), LV_PART_MAIN);
    }

    lv_obj_set_size(st, w, 28);
    lv_obj_set_style_text_align(st, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_set_pos(st, x, y);
    if (out) {
        *out = st;
    }
}

static void create_device_status(ui_home_t *ui)
{
    const int32_t px = 20;
    const int32_t py = 330;
    const int32_t pw = 170;

    create_status_label(ui->screen, px, py + 70, pw, "运行",  &ui->lbl_dev_fan);
    create_status_label(ui->screen, px, py + 115, pw, "运行", &ui->lbl_dev_heater);
    create_status_label(ui->screen, px, py + 155, pw, "运行", &ui->lbl_dev_humid);
    create_status_label(ui->screen, px, py + 200, pw, "开", &ui->lbl_dev_exhaust);
}

static void create_valve_status(ui_home_t *ui)
{
    const int32_t px = 556;
    const int32_t py = 330;
    const int32_t pw = 232;

    create_status_label(ui->screen, px, py + 70, pw, "开", &ui->lbl_valve_101);
    create_status_label(ui->screen, px, py + 115, pw, "关",  &ui->lbl_valve_102);
    create_status_label(ui->screen, px, py + 155, pw, "开", &ui->lbl_valve_103);
    create_status_label(ui->screen, px, py + 197, pw, "故障", &ui->lbl_valve_104);
}

#define MINI_GAUGE_NEEDLE_LEN   62

/**
 * ROUND_INNER 的 lv_scale 默认会在 LV_PART_MAIN 上画一整圈 arc（看起来像外圈圆环）。
 * 仅保留 line 指针时，须把 MAIN 的 arc 与 ITEMS/INDICATOR 刻度线全部关掉。
 */
static void style_scale_needle_only(lv_obj_t *scale)
{
    lv_obj_set_style_bg_opa(scale, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(scale, 0, LV_PART_MAIN);
    lv_obj_set_style_arc_width(scale, 0, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(scale, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_line_width(scale, 0, LV_PART_ITEMS);
    lv_obj_set_style_line_opa(scale, LV_OPA_TRANSP, LV_PART_ITEMS);
    lv_obj_set_style_line_width(scale, 0, LV_PART_INDICATOR);
    lv_obj_set_style_line_opa(scale, LV_OPA_TRANSP, LV_PART_INDICATOR);
}

static void create_mini_gauge_needle(lv_obj_t *box, int32_t size,
                                     int32_t range_min, int32_t range_max, int32_t value,
                                     lv_obj_t **scale_out, lv_obj_t **needle_out)
{
    lv_obj_t *scale = lv_scale_create(box);
    lv_obj_set_size(scale, size, size);
    lv_obj_center(scale);
    lv_scale_set_mode(scale, LV_SCALE_MODE_ROUND_INNER);
    lv_scale_set_label_show(scale, false);
    lv_scale_set_total_tick_count(scale, 31);
    lv_scale_set_range(scale, range_min, range_max);
    lv_scale_set_angle_range(scale, 270);
    lv_scale_set_rotation(scale, 135);
    style_scale_needle_only(scale);

    lv_obj_t *needle = lv_line_create(scale);
    lv_obj_set_style_line_width(needle, 3, LV_PART_MAIN);
    lv_obj_set_style_line_color(needle, lv_color_hex(COL_ARC_IND), LV_PART_MAIN);
    lv_obj_set_style_line_rounded(needle, true, LV_PART_MAIN);
    lv_scale_set_line_needle_value(scale, needle, MINI_GAUGE_NEEDLE_LEN, value);

    if (scale_out) {
        *scale_out = scale;
    }
    if (needle_out) {
        *needle_out = needle;
    }
}

static void create_rpm_gauge(ui_home_t *ui)
{
    const int32_t size = 200;
    const int32_t x = 310;
    const int32_t y = 320;

    lv_obj_t *box = lv_obj_create(ui->screen);
    lv_obj_set_pos(box, x, y);
    lv_obj_set_size(box, size, size);
    lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(box, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(box, 0, LV_PART_MAIN);
    lv_obj_set_scrollable(box, false);

    ui->scale_rpm = lv_scale_create(box);
    lv_obj_set_size(ui->scale_rpm, size, size);
    lv_obj_center(ui->scale_rpm);
    lv_scale_set_mode(ui->scale_rpm, LV_SCALE_MODE_ROUND_INNER);
    lv_scale_set_label_show(ui->scale_rpm, false);
    lv_scale_set_total_tick_count(ui->scale_rpm, 51);
    lv_scale_set_range(ui->scale_rpm, UI_HOME_RPM_RANGE_MIN, UI_HOME_RPM_RANGE_MAX);
    lv_scale_set_angle_range(ui->scale_rpm, 270);
    lv_scale_set_rotation(ui->scale_rpm, 135);
    style_scale_needle_only(ui->scale_rpm);

    ui->needle_rpm = lv_line_create(ui->scale_rpm);
    lv_obj_set_style_line_width(ui->needle_rpm, 4, LV_PART_MAIN);
    lv_obj_set_style_line_color(ui->needle_rpm, lv_color_hex(COL_ARC_IND), LV_PART_MAIN);
    lv_obj_set_style_line_rounded(ui->needle_rpm, true, LV_PART_MAIN);
    lv_scale_set_line_needle_value(ui->scale_rpm, ui->needle_rpm, UI_HOME_RPM_NEEDLE_LEN,
                                   UI_HOME_RPM_VALUE_INIT);

    ui->lbl_rpm_val = lv_label_create(box);
    char rpm_init[16];
    snprintf(rpm_init, sizeof(rpm_init), "%ld", (long)0);
    lv_label_set_text(ui->lbl_rpm_val, rpm_init);
    style_value_white(ui->lbl_rpm_val);
    lv_obj_align(ui->lbl_rpm_val, LV_ALIGN_CENTER, 0, 12);
}

static void create_sidebar(ui_home_t *ui)
{
    lv_obj_t *col = lv_obj_create(ui->screen);
    lv_obj_set_size(col, 132, 520);
    lv_obj_align(col, LV_ALIGN_TOP_RIGHT, -8, 64);
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(col, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(col, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, 10, LV_PART_MAIN);
    lv_obj_set_scrollable(col, false);

    ui->btn_start = lv_button_create(col);
    lv_obj_set_size(ui->btn_start, 118, 52);
    style_hit_area(ui->btn_start);

    ui->btn_power = lv_button_create(col);
    lv_obj_set_size(ui->btn_power, 52, 52);
    style_hit_area(ui->btn_power);

    ui->btn_stop = lv_button_create(col);
    lv_obj_set_size(ui->btn_stop, 118, 52);
    style_hit_area(ui->btn_stop);

    ui->btn_mode = lv_button_create(col);
    lv_obj_set_size(ui->btn_mode, 118, 52);
    style_hit_area(ui->btn_mode);

    ui->btn_trend = lv_button_create(col);
    lv_obj_set_size(ui->btn_trend, 118, 46);
    style_hit_area(ui->btn_trend);

    ui->btn_alarm = lv_button_create(col);
    lv_obj_set_size(ui->btn_alarm, 118, 46);
    style_hit_area(ui->btn_alarm);

    ui->btn_settings = lv_button_create(col);
    lv_obj_set_size(ui->btn_settings, 118, 46);
    style_hit_area(ui->btn_settings);

    ui->btn_logout = lv_button_create(col);
    lv_obj_add_event_cb(ui->btn_logout, btn_logout_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_set_size(ui->btn_logout, 118, 46);
    style_hit_area(ui->btn_logout);
}

static void create_footer_overlay(ui_home_t *ui)
{
    ui->lbl_footer_alarm = lv_label_create(ui->screen);
    lv_label_set_text(ui->lbl_footer_alarm, "未确认报警：1");
    style_value_cyan(ui->lbl_footer_alarm);
    lv_obj_align(ui->lbl_footer_alarm, LV_ALIGN_BOTTOM_MID, 72, -14);
}

void ui_home_create(ui_home_t *ui, const lv_image_dsc_t *bg)
{
    if (!ui || !bg) 
    {
        return;
    }

    ui->demo_timer = NULL;
    ui->scale_co2_needle = NULL;
    ui->needle_co2 = NULL;
    ui->scale_temp_needle = NULL;
    ui->needle_temp = NULL;
    ui->scale_hum_needle = NULL;
    ui->needle_hum = NULL;

    ui->screen = lv_obj_create(NULL);

    if (!ui->screen) 
    {
        ESP_LOGE(TAG, "lv_obj_create(screen) failed");
        return;
    }
    style_screen(ui->screen);

    ui->bg_img = lv_image_create(ui->screen);

    if (!ui->bg_img) 
    {
        ESP_LOGE(TAG, "lv_image_create failed");
        return;
    }

    lv_image_set_src(ui->bg_img, bg);
    lv_obj_set_size(ui->bg_img, LCD_H_RES, LCD_V_RES);
    lv_obj_align(ui->bg_img, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_clickable(ui->bg_img, false);
    lv_obj_set_scrollable(ui->bg_img, false);

    create_header_overlay(ui->screen, ui);

    if(ui->co2_box == NULL)
    {
        char co2_init[12];
        snprintf(co2_init, sizeof(co2_init), "%.1f", (double)UI_HOME_CO2_VALUE_INIT);
        ui->co2_box = create_mini_gauge(ui->screen, 57, 85, 200, co2_init,
                                          UI_HOME_CO2_RANGE_MIN, UI_HOME_CO2_RANGE_MAX,
                                          UI_HOME_CO2_VALUE_INIT,
                                          &ui->arc_co2, &ui->lbl_co2_val);
        create_mini_gauge_needle(ui->co2_box, 200, UI_HOME_CO2_RANGE_MIN, UI_HOME_CO2_RANGE_MAX,
                                 UI_HOME_CO2_VALUE_INIT,
                                &ui->scale_co2_needle, &ui->needle_co2);
    }
    
    if(ui->temp_box == NULL)
    {
        ui->temp_box = create_mini_gauge(ui->screen, 310, 85, 200, "0.0",
                                           UI_HOME_TEMP_RANGE_MIN, UI_HOME_TEMP_RANGE_MAX, UI_HOME_TEMP_VALUE_INIT,
                                           &ui->arc_temp, &ui->lbl_temp_val);
        create_mini_gauge_needle(ui->temp_box, 200, UI_HOME_TEMP_RANGE_MIN, UI_HOME_TEMP_RANGE_MAX, UI_HOME_TEMP_VALUE_INIT,
                                &ui->scale_temp_needle, &ui->needle_temp);
    }
    
    if(ui->hum_box == NULL)
    {
        ui->hum_box = create_mini_gauge(ui->screen, 605, 85, 200, "0",
                                          UI_HOME_HUM_RANGE_MIN, UI_HOME_HUM_RANGE_MAX, UI_HOME_HUM_VALUE_INIT,
                                          &ui->arc_hum, &ui->lbl_hum_val);
        create_mini_gauge_needle(ui->hum_box, 200, UI_HOME_HUM_RANGE_MIN, UI_HOME_HUM_RANGE_MAX, UI_HOME_HUM_VALUE_INIT,
                             &ui->scale_hum_needle, &ui->needle_hum);
    }

    
    create_device_status(ui);
    create_valve_status(ui);
    create_rpm_gauge(ui);
    create_sidebar(ui);
    create_footer_overlay(ui);

    ui_home_events_register(ui);

    ESP_LOGI(TAG, "screen home (ui_home_back + overlay widgets)");
}

void ui_home_show(const ui_home_t *ui)
{
    if (!ui || !ui->screen) 
    {
        return;
    }

    lv_screen_load(ui->screen);
}

void ui_home_destroy(ui_home_t *ui)
{
    if (!ui) 
    {
        return;
    }

    ui_home_events_unregister(ui);

    if (ui->screen) 
    {
        lv_obj_delete(ui->screen);
        ui->screen = NULL;
        ui->bg_img = NULL;
        ui->lbl_time = NULL;
        ui->img_wifi = NULL;
        ui->lbl_mode = NULL;
        ui->lbl_footer_alarm = NULL;
        ui->arc_co2 = NULL;
        ui->arc_temp = NULL;
        ui->arc_hum = NULL;
        ui->scale_co2_needle = NULL;
        ui->needle_co2 = NULL;
        ui->scale_temp_needle = NULL;
        ui->needle_temp = NULL;
        ui->scale_hum_needle = NULL;
        ui->needle_hum = NULL;
        ui->lbl_co2_val = NULL;
        ui->lbl_temp_val = NULL;
        ui->lbl_hum_val = NULL;
        ui->scale_rpm = NULL;
        ui->needle_rpm = NULL;
        ui->lbl_rpm_val = NULL;
        ui->lbl_dev_fan = NULL;
        ui->lbl_dev_heater = NULL;
        ui->lbl_dev_humid = NULL;
        ui->lbl_dev_exhaust = NULL;
        ui->lbl_valve_101 = NULL;
        ui->lbl_valve_102 = NULL;
        ui->lbl_valve_103 = NULL;
        ui->lbl_valve_104 = NULL;
        ui->btn_start = NULL;
        ui->btn_power = NULL;
        ui->btn_stop = NULL;
        ui->btn_mode = NULL;
        ui->btn_trend = NULL;
        ui->btn_alarm = NULL;
        ui->btn_settings = NULL;
        ui->btn_logout = NULL;
        ui->demo_timer = NULL;
        ui->co2_box = NULL;
        ui->temp_box = NULL;
        ui->hum_box = NULL;
    }
}
