#pragma once

#include "ui_login.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_login_events_register(ui_login_t *ui);

void ui_login_events_unregister(ui_login_t *ui);

/** 切回登录屏后恢复图表演示等状态 */
void ui_login_events_on_screen_active(ui_login_t *ui);

#ifdef __cplusplus
}
#endif
