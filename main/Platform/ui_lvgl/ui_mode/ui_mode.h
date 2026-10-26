#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *bg_img;
    lv_obj_t *panel_info;
    lv_obj_t *panel_bar;
    lv_obj_t *lbl_title;
    lv_obj_t *lbl_mode_line;
    lv_obj_t *lbl_hint;
    lv_obj_t *btn_auto;
    lv_obj_t *btn_manual;
    lv_obj_t *btn_back;
} ui_mode_t;

void ui_mode_create(ui_mode_t *ui, const lv_image_dsc_t *bg);
void ui_mode_show(const ui_mode_t *ui);
void ui_mode_destroy(ui_mode_t *ui);
void ui_mode_refresh(ui_mode_t *ui);

void ui_mode_on_screen_active(ui_mode_t *ui);
void ui_mode_on_screen_leave(ui_mode_t *ui);

#ifdef __cplusplus
}
#endif
