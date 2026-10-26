#include "ui_trend.h"

#include "app_history_store.h"
#include "ui_trend_events.h"
#include "ui_common.h"

#include "display_config.h"
#include "driver_co2.h"
#include "driver_hum.h"
#include "driver_rpm.h"
#include "driver_temp.h"
#include "esp_log.h"
#include "esp_system.h"

#include <math.h>
#include <stdio.h>
#include <time.h>

static const char *TAG = "ui_trend";

LV_FONT_DECLARE(ui_font_home_16);
LV_FONT_DECLARE(ui_font_home_22);
LV_FONT_DECLARE(lv_font_montserrat_14);

static ui_trend_t *s_ui;
static ui_trend_channel_t s_pending_channel = UI_TREND_CO2;

#define TREND_WALL_CLOCK_MIN  1577836800U

#define TREND_SCREEN_PAD     10
#define TREND_TOP_CH_H       50
#define TREND_RIGHT_W        140
#define TREND_BOTTOM_H       48
#define TREND_X_TICK_ROW_H   26
#define TREND_Y_AXIS_W       84

#define TREND_POINT_CNT_MIN  100
#define TREND_POINT_CNT_MAX  200
#define TREND_POINT_CNT_DEF  140

static float s_hist_values[TREND_POINT_CNT_MAX];
static uint32_t s_hist_ts[TREND_POINT_CNT_MAX];
static size_t s_hist_count;

#define TREND_MAIN_W         (LCD_H_RES - 2 * TREND_SCREEN_PAD - TREND_RIGHT_W)
#define TREND_PLOT_H         (LCD_V_RES - 2 * TREND_SCREEN_PAD - TREND_TOP_CH_H - TREND_BOTTOM_H - 6)
#define TREND_CHART_W        (TREND_MAIN_W - TREND_Y_AXIS_W - 4)
#define TREND_CHART_H        (TREND_PLOT_H - TREND_X_TICK_ROW_H - 4)

static const char *const s_channel_btn_text[UI_TREND_CHANNEL_COUNT] = {
    "CO2", "温度", "湿度", "转速",
};

typedef struct {
    const char *title;
    const char *unit;
    int32_t y_min;
    int32_t y_max;
    uint32_t color;
    float wave_mid;
    float wave_amp;
    float wave_phase_deg;
} ui_trend_meta_t;

static const ui_trend_meta_t s_meta[UI_TREND_CHANNEL_COUNT] = {
    { "CO2 趋势", "ppm", 350, 1200, 0x26C6DA, 780.0f, 120.0f, 0.0f },
    { "温度趋势", "℃", 18, 32, 0xFFB74D, 25.0f, 4.0f, 40.0f },
    { "湿度趋势", "%", 40, 80, 0x4FC3F7, 60.0f, 12.0f, 80.0f },
    { "转速趋势", "rpm", 1200, 1600, 0x3DDC84, 1420.0f, 80.0f, 120.0f },
};

static const lv_font_t *font_body(void)
{
    return &ui_font_home_16;
}

static const lv_font_t *font_title(void)
{
    return &ui_font_home_22;
}

static const ui_trend_meta_t *trend_meta(const ui_trend_t *ui)
{
    if (!ui || ui->channel >= UI_TREND_CHANNEL_COUNT) {
        return &s_meta[UI_TREND_CO2];
    }
    return &s_meta[ui->channel];
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

static void style_tool_btn(lv_obj_t *btn)
{
    if (!btn) {
        return;
    }
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1E3050), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(btn, lv_color_hex(0x0F1A28), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x3BCFFF), LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);
}

static void style_mode_btn(lv_obj_t *btn, bool active)
{
    if (!btn) {
        return;
    }
    lv_obj_set_style_bg_color(btn, lv_color_hex(active ? 0x1E6FD9 : 0x1E3050), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(btn, lv_color_hex(active ? 0x0E4FA8 : 0x0F1A28), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, lv_color_hex(active ? 0x5EDFFF : 0x3BCFFF), LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, active ? 2 : 1, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);
}

static void style_channel_btn(lv_obj_t *btn, bool active)
{
    style_mode_btn(btn, active);
}

static void trend_apply_y_view(ui_trend_t *ui);

static bool trend_channel_running(ui_trend_channel_t ch)
{
    switch (ch) {
    case UI_TREND_CO2:
        return co2_struct.En;
    case UI_TREND_TEMP:
        return temp_struct.En;
    case UI_TREND_HUM:
        return hum_struct.En;
    case UI_TREND_RPM:
        return rpm_struct.En;
    default:
        return false;
    }
}

static void trend_show_stopped_hint(ui_trend_t *ui)
{
    if (!ui || !ui->lbl_history_hint) {
        return;
    }
    lv_label_set_text(ui->lbl_history_hint, "未运行");
    lv_obj_set_hidden(ui->lbl_history_hint, false);
}

static int32_t trend_live_sample(const ui_trend_t *ui)
{
    if (!ui || !trend_channel_running(ui->channel)) {
        return LV_CHART_POINT_NONE;
    }

    float v = 0.0f;
    switch (ui->channel) {
    case UI_TREND_CO2:
        v = co2_struct.CO2_Real_Value;
        break;
    case UI_TREND_TEMP:
        v = temp_struct.Temp_Real_Value;
        break;
    case UI_TREND_HUM:
        v = hum_struct.Hum_Real_Value;
        break;
    case UI_TREND_RPM:
        v = rpm_struct.RPM_Real_Value;
        break;
    default:
        return LV_CHART_POINT_NONE;
    }
    return (int32_t)lroundf(v);
}

static void trend_refresh_channel_btns(ui_trend_t *ui)
{
    if (!ui) {
        return;
    }
    for (int i = 0; i < UI_TREND_CHANNEL_COUNT; i++) {
        if (ui->btn_channel[i]) {
            style_channel_btn(ui->btn_channel[i], ui->channel == (ui_trend_channel_t)i);
        }
    }
}

static void trend_clear_series(ui_trend_t *ui)
{
    if (!ui || !ui->chart || !ui->chart_ser) {
        return;
    }

    for (uint32_t i = 0; i < ui->point_cnt; i++) {
        lv_chart_set_value_by_id(ui->chart, ui->chart_ser, i, LV_CHART_POINT_NONE);
    }
    lv_chart_refresh(ui->chart);
}

static int32_t trend_y_value_at_display_index(const ui_trend_t *ui, uint32_t display_idx)
{
    if (!ui || !ui->chart || !ui->chart_ser || display_idx >= ui->point_cnt) {
        return LV_CHART_POINT_NONE;
    }

    const uint32_t start = lv_chart_get_x_start_point(ui->chart, ui->chart_ser);
    const uint32_t p_act = (start + display_idx) % ui->point_cnt;
    int32_t *y_arr = lv_chart_get_series_y_array(ui->chart, ui->chart_ser);
    if (!y_arr) {
        return LV_CHART_POINT_NONE;
    }
    return y_arr[p_act];
}

static size_t trend_history_slot_index(const ui_trend_t *ui, uint32_t display_idx)
{
    if (!ui || s_hist_count == 0 || ui->point_cnt == 0) {
        return SIZE_MAX;
    }
    if (display_idx + s_hist_count < (size_t)ui->point_cnt) {
        return SIZE_MAX;
    }
    return (size_t)(display_idx + s_hist_count - (size_t)ui->point_cnt);
}

static uint32_t trend_sec_at_display_index(const ui_trend_t *ui, uint32_t display_idx)
{
    if (!ui || ui->point_cnt < 1) {
        return 0;
    }
    if (ui->view_mode == UI_TREND_VIEW_HISTORY) {
        const size_t slot = trend_history_slot_index(ui, display_idx);
        if (slot == SIZE_MAX || slot >= s_hist_count) {
            return 0;
        }
        return s_hist_ts[slot];
    }
    const uint32_t points_ago = (uint32_t)(ui->point_cnt - 1U) - display_idx;
    const uint32_t sec_ago = points_ago * UI_TREND_SAMPLE_PERIOD_SEC;
    if (ui->realtime_sec >= sec_ago) {
        return ui->realtime_sec - sec_ago;
    }
    return 0;
}

static void trend_format_time_hms(uint32_t sec, char *buf, size_t buf_sz)
{
    if (sec > TREND_WALL_CLOCK_MIN) {
        const time_t t = (time_t)sec;
        struct tm tm_info;
        localtime_r(&t, &tm_info);
        snprintf(buf,
                 buf_sz,
                 "%02d:%02d:%02d",
                 tm_info.tm_hour,
                 tm_info.tm_min,
                 tm_info.tm_sec);
        return;
    }
    const uint32_t h = sec / 3600U;
    const uint32_t m = (sec % 3600U) / 60U;
    const uint32_t s = sec % 60U;
    snprintf(buf, buf_sz, "%02lu:%02lu:%02lu", (unsigned long)h, (unsigned long)m, (unsigned long)s);
}

static void trend_format_x_tick(uint32_t ts, char *buf, size_t buf_sz)
{
    if (ts > TREND_WALL_CLOCK_MIN) {
        const time_t t = (time_t)ts;
        struct tm tm_info;
        localtime_r(&t, &tm_info);
        snprintf(buf, buf_sz, "%02d:%02d", tm_info.tm_hour, tm_info.tm_min);
        return;
    }
    const unsigned long mins = (unsigned long)(ts / UI_TREND_SAMPLE_PERIOD_SEC);
    snprintf(buf, buf_sz, "%lum", mins);
}

void ui_trend_clear_point_info(ui_trend_t *ui)
{
    if (!ui) {
        return;
    }
    ui->sel_point_id = (uint32_t)LV_CHART_POINT_NONE;
    if (ui->lbl_point_info) {
        lv_obj_set_hidden(ui->lbl_point_info, true);
    }
    if (ui->point_marker) {
        lv_obj_set_hidden(ui->point_marker, true);
    }
    if (ui->chart && ui->chart_cursor) {
        lv_chart_set_cursor_point(ui->chart, ui->chart_cursor, ui->chart_ser, (uint32_t)LV_CHART_POINT_NONE);
    }
}

static void trend_refresh_point_info(ui_trend_t *ui)
{
    if (!ui || !ui->lbl_point_info) {
        return;
    }
    if (ui->sel_point_id == (uint32_t)LV_CHART_POINT_NONE || ui->sel_point_id >= ui->point_cnt) {
        ui_trend_clear_point_info(ui);
        return;
    }
    const int32_t v = trend_y_value_at_display_index(ui, ui->sel_point_id);
    if (v == LV_CHART_POINT_NONE) {
        lv_label_set_text(ui->lbl_point_info, "数值：--  时间：--");
        lv_obj_set_hidden(ui->lbl_point_info, false);
        return;
    }

    const ui_trend_meta_t *meta = trend_meta(ui);
    const uint32_t tsec = trend_sec_at_display_index(ui, ui->sel_point_id);
    char tbuf[16];
    char buf[64];
    trend_format_time_hms(tsec, tbuf, sizeof(tbuf));
    if (ui->view_mode == UI_TREND_VIEW_HISTORY) {
        snprintf(buf, sizeof(buf), "数值：%.1f %s  时间：%s", (double)v, meta->unit, tbuf);
    } else {
        snprintf(buf, sizeof(buf), "数值：%ld %s  时间：%s", (long)v, meta->unit, tbuf);
    }
    lv_label_set_text(ui->lbl_point_info, buf);
    lv_obj_set_hidden(ui->lbl_point_info, false);

    if (ui->chart && ui->chart_cursor) {
        lv_chart_set_cursor_point(ui->chart, ui->chart_cursor, ui->chart_ser, ui->sel_point_id);
    }
    if (ui->chart && ui->point_marker && ui->chart_ser) {
        lv_point_t pos;
        lv_chart_get_point_pos_by_id(ui->chart, ui->chart_ser, ui->sel_point_id, &pos);
        lv_obj_set_pos(ui->point_marker, pos.x - 6, pos.y - 6);
        lv_obj_set_hidden(ui->point_marker, false);
    }
}

void ui_trend_on_chart_point_selected(ui_trend_t *ui, uint32_t point_id)
{
    if (!ui || !ui->chart) {
        return;
    }
    if (point_id >= ui->point_cnt) {
        return;
    }
    ui->sel_point_id = point_id;
    trend_refresh_point_info(ui);
}

void ui_trend_update_axis_labels(ui_trend_t *ui)
{
    if (!ui) {
        return;
    }

    char buf[20];

    for (int i = 0; i < UI_TREND_Y_TICK_CNT; i++) {
        if (!ui->lbl_y_tick[i]) {
            continue;
        }
        const int32_t span = ui->y_view_max - ui->y_view_min;
        const int32_t y = ui->y_view_max - (span * i) / (UI_TREND_Y_TICK_CNT - 1);
        snprintf(buf, sizeof(buf), "%ld", (long)y);
        lv_label_set_text(ui->lbl_y_tick[i], buf);
    }

    for (int i = 0; i < UI_TREND_X_TICK_CNT; i++) {
        if (!ui->lbl_x_tick[i]) {
            continue;
        }
        const uint32_t idx =
            (uint32_t)i * (ui->point_cnt - 1U) / (UI_TREND_X_TICK_CNT - 1U);
        uint32_t t = 0;
        if (ui->view_mode == UI_TREND_VIEW_HISTORY && s_hist_count > 0) {
            const size_t slot = trend_history_slot_index(ui, idx);
            if (slot != SIZE_MAX && slot < s_hist_count) {
                t = s_hist_ts[slot];
            }
        } else {
            const uint32_t points_ago = (ui->point_cnt - 1U) - idx;
            const uint32_t sec_ago = points_ago * UI_TREND_SAMPLE_PERIOD_SEC;
            if (ui->realtime_sec >= sec_ago) {
                t = ui->realtime_sec - sec_ago;
            }
        }
        trend_format_x_tick(t, buf, sizeof(buf));
        lv_label_set_text(ui->lbl_x_tick[i], buf);
    }
}

static void trend_realtime_restart(ui_trend_t *ui)
{
    if (!ui || !ui->chart || !ui->chart_ser) {
        return;
    }

    ui->realtime_sec = 0;
    ui->demo_phase = 0;

    trend_clear_series(ui);
    lv_chart_set_x_start_point(ui->chart, ui->chart_ser, 0);

    if (!trend_channel_running(ui->channel)) {
        trend_show_stopped_hint(ui);
        ui_trend_update_axis_labels(ui);
        return;
    }

    if (ui->lbl_history_hint) {
        lv_obj_set_hidden(ui->lbl_history_hint, true);
    }

    const int32_t v0 = trend_live_sample(ui);
    if (v0 != LV_CHART_POINT_NONE) {
        lv_chart_set_next_value(ui->chart, ui->chart_ser, v0);
        ui->realtime_sec = UI_TREND_SAMPLE_PERIOD_SEC;
    }
    ui_trend_update_axis_labels(ui);
}

static void trend_realtime_reset(ui_trend_t *ui)
{
    if (!ui) {
        return;
    }

    s_hist_count = 0;
    trend_realtime_restart(ui);

    if (ui->lbl_history_hint && trend_channel_running(ui->channel)) {
        lv_obj_set_hidden(ui->lbl_history_hint, true);
    }
}

static void trend_history_apply_y_range(ui_trend_t *ui, const ui_trend_meta_t *meta)
{
    if (!ui || s_hist_count == 0) {
        return;
    }

    float vmin = (float)meta->y_min;
    float vmax = (float)meta->y_max;
    for (size_t i = 0; i < s_hist_count; i++) {
        if (s_hist_values[i] < vmin) {
            vmin = s_hist_values[i];
        }
        if (s_hist_values[i] > vmax) {
            vmax = s_hist_values[i];
        }
    }
    if (vmax - vmin < 1.0f) {
        vmin -= 1.0f;
        vmax += 1.0f;
    } else {
        const float margin = (vmax - vmin) * 0.08f;
        vmin -= margin;
        vmax += margin;
    }
    ui->y_view_min = (int32_t)floorf(vmin);
    ui->y_view_max = (int32_t)ceilf(vmax);
    trend_apply_y_view(ui);
}

static void trend_history_show_stub(ui_trend_t *ui)
{
    if (!ui || !ui->chart || !ui->chart_ser) {
        return;
    }

    ui_trend_clear_point_info(ui);
    s_hist_count = 0;

    if (!trend_channel_running(ui->channel)) {
        trend_clear_series(ui);
        trend_show_stopped_hint(ui);
        return;
    }

    if (!app_history_store_is_ready()) {
        trend_clear_series(ui);
        if (ui->lbl_history_hint) {
            lv_label_set_text(ui->lbl_history_hint, "历史曲线待接入");
            lv_obj_set_hidden(ui->lbl_history_hint, false);
        }
        return;
    }

    const app_hist_channel_t hch = (app_hist_channel_t)ui->channel;
    size_t cnt = 0;
    const esp_err_t err = app_history_load_recent(hch,
                                                  ui->point_cnt,
                                                  s_hist_values,
                                                  s_hist_ts,
                                                  &cnt);
    if (err != ESP_OK || cnt == 0) {
        trend_clear_series(ui);
        if (ui->lbl_history_hint) {
            lv_label_set_text(ui->lbl_history_hint, "历史数据为空");
            lv_obj_set_hidden(ui->lbl_history_hint, false);
        }
        ESP_LOGW(TAG, "history load ch=%d: %s cnt=%u",
                 (int)hch,
                 esp_err_to_name(err),
                 (unsigned)cnt);
        return;
    }

    s_hist_count = cnt;
    const ui_trend_meta_t *meta = trend_meta(ui);

    lv_chart_set_x_start_point(ui->chart, ui->chart_ser, 0);
    trend_clear_series(ui);

    for (uint32_t i = 0; i < ui->point_cnt; i++) {
        int32_t v = LV_CHART_POINT_NONE;
        const size_t slot = trend_history_slot_index(ui, i);
        if (slot != SIZE_MAX && slot < s_hist_count) {
            v = (int32_t)lroundf(s_hist_values[slot]);
        }
        lv_chart_set_value_by_id(ui->chart, ui->chart_ser, i, v);
    }
    lv_chart_refresh(ui->chart);

    trend_history_apply_y_range(ui, meta);

    if (ui->lbl_history_hint) {
        lv_obj_set_hidden(ui->lbl_history_hint, true);
    }
    ui_trend_update_axis_labels(ui);
    ESP_LOGI(TAG, "history loaded ch=%d points=%u", (int)hch, (unsigned)cnt);
}

void ui_trend_set_view_mode(ui_trend_t *ui, ui_trend_view_mode_t mode)
{
    if (!ui) {
        return;
    }

    ui->view_mode = mode;
    style_mode_btn(ui->btn_realtime, mode == UI_TREND_VIEW_REALTIME);
    style_mode_btn(ui->btn_history, mode == UI_TREND_VIEW_HISTORY);

    if (mode == UI_TREND_VIEW_REALTIME) {
        trend_realtime_reset(ui);
    } else {
        trend_history_show_stub(ui);
    }
}

void ui_trend_realtime_tick(ui_trend_t *ui)
{
    if (!ui || ui->view_mode != UI_TREND_VIEW_REALTIME || !ui->chart || !ui->chart_ser) {
        return;
    }

    if (!trend_channel_running(ui->channel)) {
        trend_clear_series(ui);
        trend_show_stopped_hint(ui);
        return;
    }

    if (ui->lbl_history_hint) {
        lv_obj_set_hidden(ui->lbl_history_hint, true);
    }

    ui->demo_phase++;
    ui->realtime_sec += UI_TREND_SAMPLE_PERIOD_SEC;

    const int32_t v = trend_live_sample(ui);
    if (v == LV_CHART_POINT_NONE) {
        trend_clear_series(ui);
        trend_show_stopped_hint(ui);
        return;
    }
    lv_chart_set_next_value(ui->chart, ui->chart_ser, v);
    ui_trend_update_axis_labels(ui);
    if (ui->sel_point_id != (uint32_t)LV_CHART_POINT_NONE) {
        trend_refresh_point_info(ui);
    }
}

static void trend_apply_y_view(ui_trend_t *ui)
{
    if (!ui || !ui->chart) {
        return;
    }
    lv_chart_set_axis_range(ui->chart, LV_CHART_AXIS_PRIMARY_Y, ui->y_view_min, ui->y_view_max);
}

static void trend_reset_view_from_meta(ui_trend_t *ui, const ui_trend_meta_t *meta)
{
    ui->y_view_min = meta->y_min;
    ui->y_view_max = meta->y_max;
    ui->point_cnt = TREND_POINT_CNT_DEF;
    if (ui->chart) {
        lv_chart_set_point_count(ui->chart, ui->point_cnt);
    }
    trend_apply_y_view(ui);
}

void ui_trend_y_zoom_in(ui_trend_t *ui)
{
    if (!ui) {
        return;
    }
    const int32_t mid = (ui->y_view_min + ui->y_view_max) / 2;
    int32_t span = ui->y_view_max - ui->y_view_min;
    if (span <= 4) {
        return;
    }
    span = (span * 4) / 5;
    ui->y_view_min = mid - span / 2;
    ui->y_view_max = mid + span / 2;
    trend_apply_y_view(ui);
    ui_trend_update_axis_labels(ui);
}

void ui_trend_y_zoom_out(ui_trend_t *ui)
{
    if (!ui) {
        return;
    }
    const ui_trend_meta_t *meta = trend_meta(ui);
    const int32_t mid = (ui->y_view_min + ui->y_view_max) / 2;
    int32_t span = ui->y_view_max - ui->y_view_min;
    const int32_t max_span = (meta->y_max - meta->y_min) * 3;
    span = (span * 5) / 4;
    if (span > max_span) {
        span = max_span;
    }
    ui->y_view_min = mid - span / 2;
    ui->y_view_max = mid + span / 2;
    trend_apply_y_view(ui);
    ui_trend_update_axis_labels(ui);
}

static void trend_x_zoom_apply(ui_trend_t *ui, int delta)
{
    if (!ui || !ui->chart || !ui->chart_ser) {
        return;
    }

    int new_cnt = (int)ui->point_cnt + delta;
    if (new_cnt < TREND_POINT_CNT_MIN) {
        new_cnt = TREND_POINT_CNT_MIN;
    }
    if (new_cnt > TREND_POINT_CNT_MAX) {
        new_cnt = TREND_POINT_CNT_MAX;
    }
    if (new_cnt == (int)ui->point_cnt) {
        return;
    }

    ui->point_cnt = (uint8_t)new_cnt;
    lv_chart_set_point_count(ui->chart, ui->point_cnt);

    if (ui->view_mode == UI_TREND_VIEW_REALTIME) {
        trend_realtime_restart(ui);
    }
}

void ui_trend_x_zoom_in(ui_trend_t *ui)
{
    trend_x_zoom_apply(ui, -6);
}

void ui_trend_x_zoom_out(ui_trend_t *ui)
{
    trend_x_zoom_apply(ui, 6);
}

void ui_trend_apply_channel(ui_trend_t *ui, ui_trend_channel_t ch)
{
    if (!ui || ch >= UI_TREND_CHANNEL_COUNT) {
        return;
    }

    const ui_trend_meta_t *meta = &s_meta[ch];
    const bool channel_changed = (ui->channel != ch);
    ui->channel = ch;

    if (ui->lbl_title) {
        lv_label_set_text(ui->lbl_title, meta->title);
    }
    trend_refresh_channel_btns(ui);
    if (ui->lbl_unit) {
        lv_label_set_text(ui->lbl_unit, meta->unit);
    }
    if (channel_changed) {
        trend_reset_view_from_meta(ui, meta);
    }
    if (ui->chart && ui->chart_ser) {
        trend_apply_y_view(ui);
        lv_obj_set_style_line_color(ui->chart, lv_color_hex(meta->color), LV_PART_ITEMS);
    }

    if (channel_changed) {
        ui->demo_phase = 0;
        ui->realtime_sec = 0;
        ui_trend_clear_point_info(ui);
    }

    if (ui->view_mode == UI_TREND_VIEW_HISTORY) {
        trend_history_show_stub(ui);
    } else {
        trend_realtime_reset(ui);
    }
    ui_trend_update_axis_labels(ui);
}

void ui_trend_open(ui_trend_channel_t ch)
{
    if (ch >= UI_TREND_CHANNEL_COUNT) {
        ch = UI_TREND_CO2;
    }
    s_pending_channel = ch;

    ESP_LOGI(TAG, "open ch=%d s_ui=%p screen=%p active=%p heap free=%u",
             (int)ch,
             (void *)s_ui,
             s_ui ? (void *)s_ui->screen : NULL,
             (void *)lv_screen_active(),
             (unsigned)esp_get_free_heap_size());

    ui_change_screen_no_refr(UI_SCR_ui_trend);

    if (s_ui && s_ui->screen && lv_obj_is_valid(s_ui->screen)) {
        ESP_LOGI(TAG, "apply_channel after load");
        ui_trend_apply_channel(s_ui, ch);
        lv_obj_invalidate(s_ui->screen);
    } else {
        ESP_LOGW(TAG, "skip apply_channel: ui not ready");
    }
    ESP_LOGI(TAG, "open done");
}

static lv_obj_t *trend_create_text_btn(lv_obj_t *parent, const char *text)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, font_body(), LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xF0F8FF), LV_PART_MAIN);
    lv_obj_center(lbl);
    return btn;
}

void ui_trend_create(ui_trend_t *ui, const lv_image_dsc_t *bg)
{
    if (!ui || !bg) {
        return;
    }

    ui->view_mode = UI_TREND_VIEW_REALTIME;
    ui->realtime_timer = NULL;
    ui->realtime_sec = 0;
    ui->demo_phase = 0;
    ui->bg_img = NULL;
    ui->content_root = NULL;
    for (int i = 0; i < UI_TREND_CHANNEL_COUNT; i++) {
        ui->btn_channel[i] = NULL;
    }
    ui->y_view_min = s_meta[s_pending_channel].y_min;
    ui->y_view_max = s_meta[s_pending_channel].y_max;

    ui->screen = lv_obj_create(NULL);
    if (!ui->screen) {
        ESP_LOGE(TAG, "lv_obj_create(screen) failed");
        return;
    }
    style_screen(ui->screen);

    ui->bg_img = lv_image_create(ui->screen);
    lv_image_set_src(ui->bg_img, bg);
    lv_obj_set_size(ui->bg_img, LCD_H_RES, LCD_V_RES);
    lv_obj_align(ui->bg_img, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_clickable(ui->bg_img, false);
    lv_obj_set_scrollable(ui->bg_img, false);

    ui->content_root = lv_obj_create(ui->screen);
    lv_obj_set_size(ui->content_root, LCD_H_RES, LCD_V_RES);
    lv_obj_set_pos(ui->content_root, 0, 0);
    lv_obj_set_style_bg_opa(ui->content_root, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui->content_root, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(ui->content_root, 0, LV_PART_MAIN);
    lv_obj_set_scrollable(ui->content_root, false);

    ui->panel = ui->content_root;

    lv_obj_t *ch_row = lv_obj_create(ui->panel);
    lv_obj_set_size(ch_row, LCD_H_RES - 2 * TREND_SCREEN_PAD, TREND_TOP_CH_H);
    lv_obj_align(ch_row, LV_ALIGN_TOP_MID, 0, TREND_SCREEN_PAD);
    lv_obj_set_style_bg_opa(ch_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(ch_row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(ch_row, 10, LV_PART_MAIN);
    lv_obj_set_flex_flow(ch_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ch_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(ch_row, false);

    for (int i = 0; i < UI_TREND_CHANNEL_COUNT; i++) {
        ui->btn_channel[i] = trend_create_text_btn(ch_row, s_channel_btn_text[i]);
        lv_obj_set_size(ui->btn_channel[i], 118, 42);
        style_channel_btn(ui->btn_channel[i], i == (int)s_pending_channel);
    }

    lv_obj_t *mode_col = lv_obj_create(ui->panel);
    lv_obj_set_size(mode_col, TREND_RIGHT_W, TREND_TOP_CH_H + TREND_PLOT_H);
    lv_obj_align(mode_col, LV_ALIGN_TOP_RIGHT, -TREND_SCREEN_PAD, TREND_SCREEN_PAD);
    lv_obj_set_style_bg_opa(mode_col, LV_OPA_20, LV_PART_MAIN);
    lv_obj_set_style_bg_color(mode_col, lv_color_hex(0x0C1E38), LV_PART_MAIN);
    lv_obj_set_style_border_color(mode_col, lv_color_hex(0x2A8FD4), LV_PART_MAIN);
    lv_obj_set_style_border_width(mode_col, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(mode_col, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_row(mode_col, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_all(mode_col, 10, LV_PART_MAIN);
    lv_obj_set_flex_flow(mode_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(mode_col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(mode_col, false);

    ui->btn_realtime = trend_create_text_btn(mode_col, "实时曲线");
    lv_obj_set_size(ui->btn_realtime, TREND_RIGHT_W - 20, 44);
    style_mode_btn(ui->btn_realtime, true);

    ui->btn_history = trend_create_text_btn(mode_col, "历史曲线");
    lv_obj_set_size(ui->btn_history, TREND_RIGHT_W - 20, 44);
    style_mode_btn(ui->btn_history, false);

    lv_obj_t *zoom_row = lv_obj_create(ui->panel);
    lv_obj_set_size(zoom_row, LCD_H_RES - 2 * TREND_SCREEN_PAD, TREND_BOTTOM_H);
    lv_obj_align(zoom_row, LV_ALIGN_BOTTOM_MID, 0, -TREND_SCREEN_PAD);
    lv_obj_set_style_bg_opa(zoom_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(zoom_row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_column(zoom_row, 8, LV_PART_MAIN);
    lv_obj_set_flex_flow(zoom_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(zoom_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(zoom_row, false);

    ui->btn_x_zoom_in = trend_create_text_btn(zoom_row, "X轴增大");
    lv_obj_set_size(ui->btn_x_zoom_in, 148, 40);
    style_tool_btn(ui->btn_x_zoom_in);

    ui->btn_x_zoom_out = trend_create_text_btn(zoom_row, "X轴缩小");
    lv_obj_set_size(ui->btn_x_zoom_out, 148, 40);
    style_tool_btn(ui->btn_x_zoom_out);

    ui->btn_y_zoom_in = trend_create_text_btn(zoom_row, "Y轴增大");
    lv_obj_set_size(ui->btn_y_zoom_in, 148, 40);
    style_tool_btn(ui->btn_y_zoom_in);

    ui->btn_y_zoom_out = trend_create_text_btn(zoom_row, "Y轴缩小");
    lv_obj_set_size(ui->btn_y_zoom_out, 148, 40);
    style_tool_btn(ui->btn_y_zoom_out);

    ui->btn_back = trend_create_text_btn(zoom_row, "返回");
    lv_obj_set_size(ui->btn_back, 120, 40);
    style_tool_btn(ui->btn_back);

    ui->lbl_title = lv_label_create(ui->panel);
    lv_label_set_text(ui->lbl_title, s_meta[s_pending_channel].title);
    lv_obj_set_style_text_font(ui->lbl_title, font_title(), LV_PART_MAIN);
    lv_obj_set_style_text_color(ui->lbl_title, lv_color_hex(0x5EDFFF), LV_PART_MAIN);
    lv_obj_align(ui->lbl_title, LV_ALIGN_TOP_LEFT, TREND_SCREEN_PAD, TREND_SCREEN_PAD + TREND_TOP_CH_H + 2);

    ui->lbl_unit = lv_label_create(ui->panel);
    lv_label_set_text(ui->lbl_unit, s_meta[s_pending_channel].unit);
    lv_obj_set_style_text_font(ui->lbl_unit, font_body(), LV_PART_MAIN);
    lv_obj_set_style_text_color(ui->lbl_unit, lv_color_hex(0x9EB4CC), LV_PART_MAIN);
    lv_obj_align(ui->lbl_unit, LV_ALIGN_TOP_LEFT, TREND_SCREEN_PAD + 200, TREND_SCREEN_PAD + TREND_TOP_CH_H + 6);

    ui->lbl_y_axis = NULL;
    ui->lbl_x_axis = NULL;

    lv_obj_t *plot_wrap = lv_obj_create(ui->panel);
    lv_obj_set_size(plot_wrap, TREND_MAIN_W, TREND_PLOT_H);
    lv_obj_align(plot_wrap, LV_ALIGN_TOP_LEFT, TREND_SCREEN_PAD, TREND_SCREEN_PAD + TREND_TOP_CH_H + 28);
    lv_obj_set_style_bg_opa(plot_wrap, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(plot_wrap, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(plot_wrap, 0, LV_PART_MAIN);
    lv_obj_set_scrollable(plot_wrap, false);

    lv_obj_t *y_col = lv_obj_create(plot_wrap);
    lv_obj_set_size(y_col, TREND_Y_AXIS_W, TREND_CHART_H);
    lv_obj_align(y_col, LV_ALIGN_LEFT_MID, 0, -(TREND_X_TICK_ROW_H / 2));
    lv_obj_set_style_bg_opa(y_col, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(y_col, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(y_col, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(y_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(y_col, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_scrollable(y_col, false);

    for (int i = 0; i < UI_TREND_Y_TICK_CNT; i++) {
        ui->lbl_y_tick[i] = lv_label_create(y_col);
        lv_label_set_text(ui->lbl_y_tick[i], "0");
        lv_obj_set_width(ui->lbl_y_tick[i], TREND_Y_AXIS_W - 4);
        lv_obj_set_style_text_font(ui->lbl_y_tick[i], font_body(), LV_PART_MAIN);
        lv_obj_set_style_text_color(ui->lbl_y_tick[i], lv_color_hex(0x9EB4CC), LV_PART_MAIN);
        lv_obj_set_style_text_align(ui->lbl_y_tick[i], LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    }

    ui->point_cnt = TREND_POINT_CNT_DEF;
    ui->chart = lv_chart_create(plot_wrap);
    lv_obj_set_size(ui->chart, TREND_CHART_W, TREND_CHART_H);
    lv_obj_align(ui->chart, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_chart_set_type(ui->chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(ui->chart, ui->point_cnt);
    lv_chart_set_update_mode(ui->chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_div_line_count(ui->chart, 0, 0);
    lv_obj_set_style_bg_color(ui->chart, lv_color_hex(0x061018), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui->chart, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(ui->chart, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui->chart, 0, LV_PART_MAIN);
    lv_obj_set_style_line_width(ui->chart, 1, LV_PART_ITEMS);
    lv_obj_set_style_line_rounded(ui->chart, false, LV_PART_ITEMS);
    /* 仅折线：关闭 LV_PART_INDICATOR 上的点标记 */
    lv_obj_set_style_width(ui->chart, 0, LV_PART_INDICATOR);
    lv_obj_set_style_height(ui->chart, 0, LV_PART_INDICATOR);
    lv_obj_set_style_radius(ui->chart, 0, LV_PART_INDICATOR);
    lv_obj_set_style_pad_all(ui->chart, 10, LV_PART_MAIN);

    ui->chart_ser = lv_chart_add_series(ui->chart,
                                        lv_color_hex(s_meta[s_pending_channel].color),
                                        LV_CHART_AXIS_PRIMARY_Y);

    ui->chart_cursor = lv_chart_add_cursor(ui->chart, lv_color_hex(0x5EDFFF), LV_DIR_VER);
    lv_chart_set_cursor_point(ui->chart, ui->chart_cursor, ui->chart_ser, (uint32_t)LV_CHART_POINT_NONE);

    ui->point_marker = lv_obj_create(ui->chart);
    lv_obj_set_size(ui->point_marker, 12, 12);
    lv_obj_set_style_radius(ui->point_marker, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui->point_marker, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_border_color(ui->point_marker, lv_color_hex(0x5EDFFF), LV_PART_MAIN);
    lv_obj_set_style_border_width(ui->point_marker, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_all(ui->point_marker, 0, LV_PART_MAIN);
    lv_obj_set_clickable(ui->point_marker, false);
    lv_obj_set_hidden(ui->point_marker, true);

    ui->lbl_point_info = lv_label_create(plot_wrap);
    lv_label_set_text(ui->lbl_point_info, "");
    lv_obj_set_style_text_font(ui->lbl_point_info, font_body(), LV_PART_MAIN);
    lv_obj_set_style_text_color(ui->lbl_point_info, lv_color_hex(0xE8F4FF), LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui->lbl_point_info, lv_color_hex(0x0C1E38), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui->lbl_point_info, LV_OPA_80, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(ui->lbl_point_info, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(ui->lbl_point_info, 6, LV_PART_MAIN);
    lv_obj_set_style_radius(ui->lbl_point_info, 6, LV_PART_MAIN);
    lv_obj_align(ui->lbl_point_info, LV_ALIGN_TOP_MID, -(TREND_Y_AXIS_W / 2), 2);
    lv_obj_set_hidden(ui->lbl_point_info, true);

    ui->lbl_history_hint = lv_label_create(ui->chart);
    lv_label_set_text(ui->lbl_history_hint, "历史曲线待接入");
    lv_obj_set_style_text_font(ui->lbl_history_hint, font_body(), LV_PART_MAIN);
    lv_obj_set_style_text_color(ui->lbl_history_hint, lv_color_hex(0x9EB4CC), LV_PART_MAIN);
    lv_obj_center(ui->lbl_history_hint);
    lv_obj_set_hidden(ui->lbl_history_hint, true);

    ui->sel_point_id = (uint32_t)LV_CHART_POINT_NONE;

    lv_obj_t *x_row = lv_obj_create(plot_wrap);
    lv_obj_set_size(x_row, TREND_CHART_W, TREND_X_TICK_ROW_H);
    lv_obj_align(x_row, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_bg_opa(x_row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(x_row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(x_row, 0, LV_PART_MAIN);
    lv_obj_set_flex_flow(x_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(x_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(x_row, false);

    for (int i = 0; i < UI_TREND_X_TICK_CNT; i++) {
        ui->lbl_x_tick[i] = lv_label_create(x_row);
        lv_label_set_text(ui->lbl_x_tick[i], "0s");
        lv_obj_set_style_text_font(ui->lbl_x_tick[i], &lv_font_montserrat_14, LV_PART_MAIN);
        lv_obj_set_style_text_color(ui->lbl_x_tick[i], lv_color_hex(0x9EB4CC), LV_PART_MAIN);
    }

    ui_trend_bind_click_dismiss(ch_row, ui);
    ui_trend_bind_click_dismiss(mode_col, ui);
    ui_trend_bind_click_dismiss(zoom_row, ui);
    ui_trend_bind_click_dismiss(plot_wrap, ui);
    ui_trend_bind_click_dismiss(y_col, ui);
    ui_trend_bind_click_dismiss(x_row, ui);
    ui_trend_bind_click_dismiss(ui->lbl_title, ui);
    ui_trend_bind_click_dismiss(ui->lbl_unit, ui);
    ui_trend_bind_click_dismiss(ui->lbl_point_info, ui);

    ui->channel = UI_TREND_CHANNEL_COUNT;
    s_ui = ui;
    ui_trend_apply_channel(ui, s_pending_channel);
    ui_trend_events_register(ui);

    ESP_LOGI(TAG, "ui_trend created");
}

void ui_trend_show(const ui_trend_t *ui)
{
    if (!ui || !ui->screen) {
        return;
    }
    lv_screen_load(ui->screen);
}

void ui_trend_destroy(ui_trend_t *ui)
{
    if (!ui) {
        return;
    }

    ui_trend_events_unregister(ui);

    if (ui->screen) {
        lv_obj_delete(ui->screen);
        ui->screen = NULL;
        ui->bg_img = NULL;
        ui->panel = NULL;
        ui->lbl_title = NULL;
        ui->lbl_unit = NULL;
        ui->chart = NULL;
        ui->chart_ser = NULL;
        ui->btn_back = NULL;
        ui->btn_realtime = NULL;
        ui->btn_history = NULL;
        ui->btn_y_zoom_in = NULL;
        ui->btn_y_zoom_out = NULL;
        ui->btn_x_zoom_in = NULL;
        ui->btn_x_zoom_out = NULL;
        ui->lbl_y_axis = NULL;
        ui->lbl_x_axis = NULL;
        ui->lbl_history_hint = NULL;
        ui->chart_cursor = NULL;
        ui->lbl_point_info = NULL;
        ui->point_marker = NULL;
    }

    if (s_ui == ui) {
        s_ui = NULL;
    }
}
