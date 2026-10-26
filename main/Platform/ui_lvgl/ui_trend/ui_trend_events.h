#pragma once

#include "ui_trend.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_trend_events_register(ui_trend_t *ui);

void ui_trend_events_unregister(ui_trend_t *ui);

/** 点击该区域（且不在曲线图内）时清除数值/时间提示 */
void ui_trend_bind_click_dismiss(lv_obj_t *obj, ui_trend_t *ui);

#ifdef __cplusplus
}
#endif
