#ifndef UI_COMMON_H
#define UI_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

#include <stdbool.h>

/**
 * 界面切换：ui_init() 时一次性预创建 login/home/mode/wifi/trend/alarm/param 共 UI_SCR_COUNT 个屏，
 * 运行期间对象树常驻，ui_change_screen* 仅 lv_screen_load + 事件回调，不销毁界面。
 */

typedef enum {
    UI_SCR_ui_login = 0,
    UI_SCR_ui_home,
    UI_SCR_ui_mode,
    UI_SCR_ui_wifi,
    UI_SCR_ui_trend,
    UI_SCR_ui_alarm,
    UI_SCR_ui_param,
    UI_SCR_COUNT
} ui_screen_id_t;

void ui_init(void);

/** ui_init 预创建完成后为 true */
bool ui_all_screens_created(void);

void ui_change_screen(ui_screen_id_t id);
/** 切屏但不 lv_refr_now，避免与 top 层弹窗销毁同一拍全屏刷新 */
void ui_change_screen_no_refr(ui_screen_id_t id);
/** 登录成功后进入主页：分阶段显示，避免首帧全屏 HMI + 背景同拍刷新 */
void ui_goto_home_after_login(void);
ui_screen_id_t ui_get_current_screen(void);

/** 兼容旧接口：仅加载已创建对象，不推荐新代码使用 */
void ui_load_screen(lv_obj_t *scr);

/** 在屏幕顶部居中挂 FPS / CPU 标签（读 common.h 中 g_ui_fps、g_ui_cpu_usage_pct） */
void ui_common_attach_perf_overlay(lv_obj_t *screen, ui_screen_id_t id);

/** 将主页「模式：自动/手动」标签与 app_run_mode 同步 */
void ui_home_sync_mode_label(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_COMMON_H */
