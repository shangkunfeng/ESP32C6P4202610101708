#pragma once

#include "ui_param.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_param_events_register(ui_param_t *ui);

void ui_param_events_on_screen_active(ui_param_t *ui);
void ui_param_events_on_screen_leave(ui_param_t *ui);

/** 白底数值框（lv_textarea）点击后弹出数字键盘 */
void ui_param_events_attach_value(lv_obj_t *ta);

/** 左侧 lv_list 导航按钮 */
void ui_param_events_attach_nav_button(lv_obj_t *btn, ui_param_group_t group);

void ui_param_events_attach_brightness_slider(ui_param_t *ui);

#ifdef __cplusplus
}
#endif
