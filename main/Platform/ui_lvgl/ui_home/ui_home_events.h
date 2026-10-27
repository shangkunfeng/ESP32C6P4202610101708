#pragma once

#include <stdbool.h>

#include "ui_home.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_home_events_register(ui_home_t *ui);

void ui_home_events_unregister(ui_home_t *ui);

/** 主页再次被 load 时恢复演示动画 */
void ui_home_events_on_screen_active(ui_home_t *ui);

void ui_home_events_on_screen_leave(ui_home_t *ui);

/** 关闭趋势弹层等，切到其它屏前调用 */
void ui_home_events_dismiss_popups(ui_home_t *ui);

/** 从 co2/temp/hum/rpm_struct 刷新表盘数字与指针（En 为 false 时显示 0） */
void ui_home_refresh_real_values(ui_home_t *ui);

/** 登录成功切主页：先纯色底，再背景与控件分步显示 */
void ui_home_events_arm_login_staged(ui_home_t *ui);

/** 登录分阶段进主页时，切屏处勿整屏 invalidate */
bool ui_home_events_login_staged_armed(void);

/* 退出按键事件 */
void btn_logout_cb(lv_event_t *e);

#ifdef __cplusplus
}
#endif
