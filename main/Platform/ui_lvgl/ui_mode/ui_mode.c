#include "ui_mode.h"

#include "app_run_mode.h"
#include "display_config.h"
#include "ui_mode_events.h"

#include "esp_log.h"

static const char *TAG = "ui_mode";

LV_FONT_DECLARE(ui_font_home_22);

#define COL_CYAN   0x5EDFFF
#define COL_WHITE  0xF0F8FF
#define COL_MUTED  0xC8D8E8

static void style_screen(lv_obj_t *screen)
{
    lv_obj_set_scrollable(screen, false);
    lv_obj_set_size(screen, LCD_H_RES, LCD_V_RES);
    lv_obj_set_style_pad_all(screen, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(screen, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(screen, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_TRANSP, LV_PART_MAIN);
}

static void style_option_btn(lv_obj_t *btn, bool active)
{
    lv_obj_set_style_bg_color(btn, lv_color_hex(active ? 0x1E6FD9 : 0x1E3050), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(btn, lv_color_hex(active ? 0x0E4FA8 : 0x0F1A28), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, lv_color_hex(active ? 0x5EDFFF : 0x3BCFFF), LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, active ? 2 : 1, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_80, LV_PART_MAIN);
}

static void style_tool_btn(lv_obj_t *btn)
{
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1A2838), LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x3BCFFF), LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_80, LV_PART_MAIN);
}

static lv_obj_t *create_text_btn(lv_obj_t *parent, const char *text)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &ui_font_home_22, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_hex(COL_WHITE), LV_PART_MAIN);
    lv_obj_center(lbl);
    return btn;
}

static const char *mode_description(void)
{
    if (app_run_mode_get() == APP_RUN_MODE_AUTO) {
        return "自动：按参数设定自动调节设备。\n当前可在主页查看运行状态。";
    }
    return "手动：由操作员干预控制。\n自动调节暂停，请谨慎操作。";
}

void ui_mode_refresh(ui_mode_t *ui)
{
    if (!ui) {
        return;
    }
    const bool is_auto = (app_run_mode_get() == APP_RUN_MODE_AUTO);
    style_option_btn(ui->btn_auto, is_auto);
    style_option_btn(ui->btn_manual, !is_auto);
    if (ui->lbl_mode_line) {
        lv_label_set_text(ui->lbl_mode_line, app_run_mode_line_text());
    }
    if (ui->lbl_hint) {
        lv_label_set_text(ui->lbl_hint, mode_description());
    }
}

void ui_mode_create(ui_mode_t *ui, const lv_image_dsc_t *bg)
{
    if (!ui) {
        return;
    }

    ui->screen = NULL;
    ui->bg_img = NULL;
    ui->panel_info = NULL;
    ui->panel_bar = NULL;
    ui->lbl_title = NULL;
    ui->lbl_mode_line = NULL;
    ui->lbl_hint = NULL;
    ui->btn_auto = NULL;
    ui->btn_manual = NULL;
    ui->btn_back = NULL;

    ui->screen = lv_obj_create(NULL);
    if (!ui->screen) {
        ESP_LOGE(TAG, "lv_obj_create(screen) failed");
        return;
    }
    style_screen(ui->screen);

    if (bg) {
        ui->bg_img = lv_image_create(ui->screen);
        lv_image_set_src(ui->bg_img, bg);
        lv_obj_set_size(ui->bg_img, LCD_H_RES, LCD_V_RES);
        lv_obj_align(ui->bg_img, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_set_clickable(ui->bg_img, false);
    }

    ui->lbl_title = lv_label_create(ui->screen);
    lv_label_set_text(ui->lbl_title, "运行模式设置");
    lv_obj_set_style_text_font(ui->lbl_title, &ui_font_home_22, LV_PART_MAIN);
    lv_obj_set_style_text_color(ui->lbl_title, lv_color_hex(COL_CYAN), LV_PART_MAIN);
    lv_obj_align(ui->lbl_title, LV_ALIGN_TOP_MID, 0, 20);

    ui->panel_info = lv_obj_create(ui->screen);
    lv_obj_set_size(ui->panel_info, LCD_H_RES - 80, 200);
    lv_obj_align(ui->panel_info, LV_ALIGN_CENTER, 0, -40);
    lv_obj_set_style_bg_color(ui->panel_info, lv_color_hex(0x0A1420), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui->panel_info, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_border_color(ui->panel_info, lv_color_hex(0x2A8FD4), LV_PART_MAIN);
    lv_obj_set_style_border_width(ui->panel_info, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(ui->panel_info, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_all(ui->panel_info, 20, LV_PART_MAIN);
    lv_obj_set_scrollable(ui->panel_info, false);

    ui->lbl_mode_line = lv_label_create(ui->panel_info);
    lv_obj_set_style_text_font(ui->lbl_mode_line, &ui_font_home_22, LV_PART_MAIN);
    lv_obj_set_style_text_color(ui->lbl_mode_line, lv_color_hex(COL_CYAN), LV_PART_MAIN);
    lv_label_set_text(ui->lbl_mode_line, app_run_mode_line_text());
    lv_obj_align(ui->lbl_mode_line, LV_ALIGN_TOP_MID, 0, 0);

    ui->lbl_hint = lv_label_create(ui->panel_info);
    lv_obj_set_width(ui->lbl_hint, LCD_H_RES - 120);
    lv_label_set_long_mode(ui->lbl_hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(ui->lbl_hint, &ui_font_home_22, LV_PART_MAIN);
    lv_obj_set_style_text_color(ui->lbl_hint, lv_color_hex(COL_MUTED), LV_PART_MAIN);
    lv_obj_set_style_text_align(ui->lbl_hint, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(ui->lbl_hint, LV_ALIGN_TOP_MID, 0, 48);

    ui->panel_bar = lv_obj_create(ui->screen);
    lv_obj_set_size(ui->panel_bar, LCD_H_RES, 88);
    lv_obj_align(ui->panel_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(ui->panel_bar, lv_color_hex(0x0A1420), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui->panel_bar, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui->panel_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(ui->panel_bar, 8, LV_PART_MAIN);
    lv_obj_set_flex_flow(ui->panel_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui->panel_bar, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(ui->panel_bar, false);

    ui->btn_auto = create_text_btn(ui->panel_bar, "自动");
    lv_obj_set_size(ui->btn_auto, 160, 56);
    ui->btn_manual = create_text_btn(ui->panel_bar, "手动");
    lv_obj_set_size(ui->btn_manual, 160, 56);
    ui->btn_back = create_text_btn(ui->panel_bar, "返回");
    lv_obj_set_size(ui->btn_back, 160, 56);
    style_tool_btn(ui->btn_back);

    lv_obj_move_foreground(ui->lbl_title);
    lv_obj_move_foreground(ui->panel_info);
    lv_obj_move_foreground(ui->panel_bar);

    ui_mode_refresh(ui);
    ui_mode_events_register(ui);

    ESP_LOGI(TAG, "ui_mode created (run mode settings)");
}

void ui_mode_on_screen_active(ui_mode_t *ui)
{
    if (!ui) {
        return;
    }
    ui_mode_refresh(ui);
}

void ui_mode_on_screen_leave(ui_mode_t *ui)
{
    (void)ui;
}

void ui_mode_show(const ui_mode_t *ui)
{
    if (!ui || !ui->screen) {
        return;
    }
    lv_screen_load(ui->screen);
}

void ui_mode_destroy(ui_mode_t *ui)
{
    if (!ui) {
        return;
    }
    if (ui->screen) {
        lv_obj_delete(ui->screen);
        ui->screen = NULL;
    }
}
