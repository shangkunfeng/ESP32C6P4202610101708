#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_TREND_CO2 = 0,
    UI_TREND_TEMP,
    UI_TREND_HUM,
    UI_TREND_RPM,
    UI_TREND_CHANNEL_COUNT
} ui_trend_channel_t;

typedef enum {
    UI_TREND_VIEW_REALTIME = 0,
    UI_TREND_VIEW_HISTORY,
} ui_trend_view_mode_t;

#define UI_TREND_Y_TICK_CNT 5
#define UI_TREND_X_TICK_CNT 6

/** 曲线每个数据点的时间间隔（秒）；与历史存盘采样一致 */
#define UI_TREND_SAMPLE_PERIOD_SEC  60U

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *bg_img;
    lv_obj_t *content_root;
    lv_obj_t *panel;
    lv_obj_t *lbl_title;
    lv_obj_t *lbl_unit;
    lv_obj_t *chart;
    lv_chart_series_t *chart_ser;
    lv_obj_t *btn_channel[UI_TREND_CHANNEL_COUNT];
    lv_obj_t *btn_back;
    lv_obj_t *btn_realtime;
    lv_obj_t *btn_history;
    lv_obj_t *btn_y_zoom_in;
    lv_obj_t *btn_y_zoom_out;
    lv_obj_t *btn_x_zoom_in;
    lv_obj_t *btn_x_zoom_out;
    lv_obj_t *lbl_y_axis;
    lv_obj_t *lbl_x_axis;
    lv_obj_t *lbl_y_tick[UI_TREND_Y_TICK_CNT];
    lv_obj_t *lbl_x_tick[UI_TREND_X_TICK_CNT];
    lv_obj_t *lbl_history_hint;
    lv_chart_cursor_t *chart_cursor;
    lv_obj_t *lbl_point_info;
    lv_obj_t *point_marker;

    ui_trend_channel_t channel;
    ui_trend_view_mode_t view_mode;
    lv_timer_t *realtime_timer;
    uint16_t demo_phase;
    uint32_t realtime_sec;
    uint8_t point_cnt;
    int32_t y_view_min;
    int32_t y_view_max;
    uint32_t sel_point_id;
} ui_trend_t;

void ui_trend_on_chart_point_selected(ui_trend_t *ui, uint32_t point_id);
void ui_trend_clear_point_info(ui_trend_t *ui);

void ui_trend_create(ui_trend_t *ui, const lv_image_dsc_t *bg);
void ui_trend_show(const ui_trend_t *ui);
void ui_trend_destroy(ui_trend_t *ui);

void ui_trend_open(ui_trend_channel_t ch);

void ui_trend_apply_channel(ui_trend_t *ui, ui_trend_channel_t ch);
void ui_trend_set_view_mode(ui_trend_t *ui, ui_trend_view_mode_t mode);
void ui_trend_realtime_tick(ui_trend_t *ui);
void ui_trend_update_axis_labels(ui_trend_t *ui);
void ui_trend_y_zoom_in(ui_trend_t *ui);
void ui_trend_y_zoom_out(ui_trend_t *ui);
void ui_trend_x_zoom_in(ui_trend_t *ui);
void ui_trend_x_zoom_out(ui_trend_t *ui);

#ifdef __cplusplus
}
#endif
