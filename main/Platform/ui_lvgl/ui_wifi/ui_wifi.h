#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UI_WIFI_AP_LIST_MAX   16

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *bg_img;
    lv_obj_t *lbl_title;
    lv_obj_t *img_status;
    lv_obj_t *panel_status;
    lv_obj_t *lbl_conn_state;
    lv_obj_t *lbl_ssid;
    lv_obj_t *lbl_ip;
    lv_obj_t *panel_list;
    lv_obj_t *lbl_list_title;
    lv_obj_t *list_ap;
    lv_obj_t *panel_bar;
    lv_obj_t *btn_scan;
    lv_obj_t *btn_connect;
    lv_obj_t *btn_back;
    lv_obj_t *pwd_overlay;
    lv_obj_t *pwd_panel;
    lv_obj_t *pwd_title;
    lv_obj_t *pwd_ta;
    lv_obj_t *pwd_btn_ok;
    lv_obj_t *pwd_btn_cancel;
    lv_obj_t *pwd_keyboard;
    int selected_ap;
    int ap_display_count;
} ui_wifi_t;

void ui_wifi_create(ui_wifi_t *ui, const lv_image_dsc_t *bg);
void ui_wifi_show(const ui_wifi_t *ui);
void ui_wifi_destroy(ui_wifi_t *ui);

void ui_wifi_refresh(ui_wifi_t *ui);

void ui_wifi_on_screen_active(ui_wifi_t *ui);
void ui_wifi_on_screen_leave(ui_wifi_t *ui);

void ui_wifi_refresh_status(ui_wifi_t *ui);
void ui_wifi_refresh_ap_list(ui_wifi_t *ui);

#ifdef __cplusplus
}
#endif
