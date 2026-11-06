#pragma once

#include "app_network_hmi.h"
#include "lvgl.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *bg_img;
    lv_obj_t *lbl_title;
    lv_obj_t *img_status;
    lv_obj_t *panel_status;
    lv_obj_t *lbl_link;
    lv_obj_t *lbl_ip;
    lv_obj_t *panel_url;
    lv_obj_t *lbl_url_hint;
    lv_obj_t *ta_url;
    lv_obj_t *lbl_result;
    lv_obj_t *panel_hmi;
    lv_obj_t *lbl_hmi_title;
    lv_obj_t *lbl_hmi_hint;
    lv_obj_t *lbl_hmi_lines[APP_NETWORK_HMI_LINE_MAX];
    lv_obj_t *lbl_hmi_items[APP_NETWORK_HMI_ITEM_MAX];
    lv_obj_t *keyboard;
    lv_obj_t *panel_bar;
    lv_obj_t *btn_back;
    lv_obj_t *btn_login;
} ui_network_t;

void ui_network_create(ui_network_t *ui, const lv_image_dsc_t *bg);
void ui_network_show(const ui_network_t *ui);
void ui_network_destroy(ui_network_t *ui);

void ui_network_on_screen_active(ui_network_t *ui);
void ui_network_on_screen_leave(ui_network_t *ui);

void ui_network_refresh_status(ui_network_t *ui);
void ui_network_set_probe_busy(ui_network_t *ui, bool busy);
void ui_network_show_probe_result(ui_network_t *ui, bool success);
void ui_network_clear_hmi_panel(ui_network_t *ui);
void ui_network_show_hmi_panel(ui_network_t *ui, const app_network_hmi_view_t *view);
void ui_network_show_hmi_parse_hint(ui_network_t *ui);

#ifdef __cplusplus
}
#endif
