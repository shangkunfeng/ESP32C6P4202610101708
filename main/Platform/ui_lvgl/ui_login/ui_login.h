#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 登录屏（背景 SD: photo/ui_login.bin + 前景控件） */
typedef struct {
    lv_obj_t *screen;
    lv_obj_t *bg_img;
    lv_obj_t *panel;
    lv_obj_t *title;
    lv_obj_t *ta_user;
    lv_obj_t *ta_pass;
    lv_obj_t *btn_login;
    lv_obj_t *keyboard;
    lv_obj_t *status_box;
    lv_obj_t *chart_bar;
    lv_obj_t *chart_line;
    lv_chart_series_t *chart_bar_ser;
    lv_chart_series_t *chart_line_ser;
    uint8_t chart_point_cnt;
    uint16_t chart_demo_phase;
    lv_timer_t *chart_demo_timer;
} ui_login_t;

void ui_login_create(ui_login_t *ui, const lv_image_dsc_t *bg);

void ui_login_show(const ui_login_t *ui);

void ui_login_destroy(ui_login_t *ui);

#ifdef __cplusplus
}
#endif
