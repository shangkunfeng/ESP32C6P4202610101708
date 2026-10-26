#pragma once

#include "lvgl.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    const char *code;
    const char *message;
    uint8_t level;   /**< 1=一般，2=通讯等严重 */
    uint16_t count;  /**< 发生次数，暂未统计时可置 0 */
} ui_alarm_record_t;

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *bg_img;
    lv_obj_t *content_root;
    lv_obj_t *lbl_title;
    lv_obj_t *header_row;
    lv_obj_t *list;
    lv_obj_t *lbl_page;
    lv_obj_t *btn_prev;
    lv_obj_t *btn_next;
    lv_obj_t *btn_jump;
    lv_obj_t *btn_back;
    lv_obj_t *btn_clear;
    lv_obj_t *panel_jump;
    lv_obj_t *ta_jump_page;
    lv_obj_t *btn_jump_ok;
    lv_obj_t *keyboard;
    int current_page;
    int page_count;
} ui_alarm_t;

void ui_alarm_create(ui_alarm_t *ui, const lv_image_dsc_t *bg);
void ui_alarm_show(const ui_alarm_t *ui);
void ui_alarm_destroy(ui_alarm_t *ui);

void ui_alarm_refresh_list(ui_alarm_t *ui);
void ui_alarm_clear_all(void);

#ifdef __cplusplus
}
#endif
