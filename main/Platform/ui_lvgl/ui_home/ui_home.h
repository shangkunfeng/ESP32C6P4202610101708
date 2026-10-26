#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 转速表指针长度（px），须与 ui_home_events 里 set_line_needle_value 第三参数一致 */
#define UI_HOME_RPM_NEEDLE_LEN      40

#define UI_HOME_CO2_RANGE_MIN       0
#define UI_HOME_CO2_RANGE_MAX       100
/** CO2 表盘初值（与量程 0~100 一致，显示数字不再 /100） */
#define UI_HOME_CO2_VALUE_INIT      0
#define UI_HOME_TEMP_RANGE_MIN      0
#define UI_HOME_TEMP_RANGE_MAX      60
#define UI_HOME_TEMP_VALUE_INIT      0
#define UI_HOME_HUM_RANGE_MIN       0
#define UI_HOME_HUM_RANGE_MAX       100
#define UI_HOME_HUM_VALUE_INIT      0
#define UI_HOME_RPM_RANGE_MIN       0
#define UI_HOME_RPM_RANGE_MAX       500
#define UI_HOME_RPM_VALUE_INIT      0

/** 主页：ui_home_back.bin 全屏底图 + LVGL 动态数值/交互层 */
typedef struct {
    lv_obj_t *screen;
    lv_obj_t *bg_img;

    lv_obj_t *lbl_time;
    lv_obj_t *img_wifi;
    lv_obj_t *lbl_mode;
    lv_obj_t *lbl_footer_alarm;

    lv_obj_t *arc_co2;
    lv_obj_t *arc_temp;
    lv_obj_t *arc_hum;
    lv_obj_t *scale_co2_needle;
    lv_obj_t *needle_co2;
    lv_obj_t *scale_temp_needle;
    lv_obj_t *needle_temp;
    lv_obj_t *scale_hum_needle;
    lv_obj_t *needle_hum;
    lv_obj_t *lbl_co2_val;
    lv_obj_t *lbl_temp_val;
    lv_obj_t *lbl_hum_val;

    lv_obj_t *scale_rpm;
    lv_obj_t *needle_rpm;
    lv_obj_t *lbl_rpm_val;

    lv_obj_t *lbl_dev_fan;
    lv_obj_t *lbl_dev_heater;
    lv_obj_t *lbl_dev_humid;
    lv_obj_t *lbl_dev_exhaust;

    lv_obj_t *lbl_valve_101;
    lv_obj_t *lbl_valve_102;
    lv_obj_t *lbl_valve_103;
    lv_obj_t *lbl_valve_104;

    lv_obj_t *btn_start;
    lv_obj_t *btn_power;
    lv_obj_t *btn_stop;
    lv_obj_t *btn_mode;
    lv_obj_t *btn_trend;
    lv_obj_t *btn_alarm;
    lv_obj_t *btn_settings;
    lv_obj_t *btn_logout;
    lv_obj_t *co2_box;
    lv_obj_t *temp_box;
    lv_obj_t *hum_box;

    lv_timer_t *demo_timer;
} ui_home_t;

void ui_home_create(ui_home_t *ui, const lv_image_dsc_t *bg);

void ui_home_show(const ui_home_t *ui);

void ui_home_destroy(ui_home_t *ui);

/** 按 app_wifi 连接状态切换 wifi_no_link / wifi_link 图标 */
void ui_home_refresh_wifi_icon(ui_home_t *ui);

#ifdef __cplusplus
}
#endif
