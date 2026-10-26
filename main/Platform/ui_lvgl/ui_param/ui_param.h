#pragma once

#include "lvgl.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UI_PARAM_GROUP_CO2 = 0,
    UI_PARAM_GROUP_TEMP,
    UI_PARAM_GROUP_HUM,
    UI_PARAM_GROUP_RPM,
    UI_PARAM_GROUP_SCREEN,
    UI_PARAM_GROUP_COUNT
} ui_param_group_t;

/** @deprecated 兼容旧枚举名 */
typedef ui_param_group_t ui_param_page_t;
#define UI_PARAM_PAGE_COUNT UI_PARAM_GROUP_COUNT

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *bg_img;
    lv_obj_t *content_root;
    lv_obj_t *lbl_title;
    lv_obj_t *btn_back;
    lv_obj_t *btn_confirm;
    lv_obj_t *btn_reset;
    lv_obj_t *nav_list;
    lv_obj_t *nav_btns[UI_PARAM_GROUP_COUNT];
    lv_obj_t *panel_host;
    lv_obj_t *group_panels[UI_PARAM_GROUP_COUNT];
    ui_param_group_t active_page;
    lv_obj_t *keyboard;
    lv_obj_t *slider_brightness;
    lv_obj_t *lbl_brightness_value;
} ui_param_t;

void ui_param_create(ui_param_t *ui, const lv_image_dsc_t *bg);
void ui_param_show(const ui_param_t *ui);
void ui_param_destroy(ui_param_t *ui);

void ui_param_show_page(ui_param_t *ui, ui_param_page_t page);

void ui_param_keyboard_apply_style(lv_obj_t *keyboard);
void ui_param_keyboard_place_near_field(lv_obj_t *keyboard, lv_obj_t *field_ta);

/** 从 co2/temp/hum/rpm 结构体刷新各面板数值框 */
void ui_param_sync_from_drivers(ui_param_t *ui);
/** 将各面板数值框写回结构体（不含 Real_*） */
void ui_param_sync_to_drivers(ui_param_t *ui);

/** 单格：超出上下限则改为限值并刷新显示 */
void ui_param_clamp_textarea(lv_obj_t *ta);
/** 四个模块全部数值框限幅 */
void ui_param_clamp_all_fields(ui_param_t *ui);

/** 屏幕亮度滑条 ↔ board_get/set_backlight */
void ui_param_sync_screen_brightness(ui_param_t *ui, bool to_hardware);

#ifdef __cplusplus
}
#endif
