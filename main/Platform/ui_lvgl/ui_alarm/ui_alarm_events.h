#pragma once

#include "ui_alarm.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_alarm_keyboard_setup(lv_obj_t *keyboard);
void ui_alarm_events_register(ui_alarm_t *ui);
void ui_alarm_events_on_screen_active(ui_alarm_t *ui);

#ifdef __cplusplus
}
#endif
