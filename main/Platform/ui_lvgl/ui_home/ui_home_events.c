#include "ui_home_events.h"

#include "app_lvgl.h"
#include "ui_common.h"
#include "ui_trend.h"

#include "esp_log.h"
#include "esp_system.h"

#include <stdint.h>
#include <stdio.h>
#include "driver_co2.h"
#include "driver_hum.h"
#include "driver_rpm.h"
#include "driver_temp.h"

static const char *TAG = "ui_home_evt";

LV_FONT_DECLARE(ui_font_home_16);
LV_FONT_DECLARE(ui_font_home_22);

#define HOME_DEMO_PERIOD_MS 1200

#define TREND_POPUP_BUILD_TAG "screen-overlay-v9"

#define TREND_PANEL_W 480
#define TREND_PANEL_H 420

static lv_obj_t *s_trend_overlay;
static ui_home_t *s_trend_home_ui;
static lv_timer_t *s_trend_nav_timer;
static ui_trend_channel_t s_trend_nav_channel;

static bool s_home_login_staged_armed;
static ui_home_t *s_home_login_staged_ui;
static lv_timer_t *s_home_login_stage_timer;
static lv_timer_t *s_home_login_demo_resume_timer;
static uint8_t s_home_login_stage_phase;
static bool s_home_login_entry_complete;

#define HOME_LOGIN_BG_STEP_MS        80
#define HOME_LOGIN_WIDGETS_STEP_MS   80
#define HOME_LOGIN_DEMO_RESUME_MS    150

static const char *const s_trend_opts[] = { "CO2", "温度", "湿度", "转速" };

static void trend_nav_timer_cancel(void);
static void trend_overlay_close_sync(bool resume_home_demo);
static void trend_home_screen_slim(ui_home_t *ui, bool slim);
static void trend_popup_show(ui_home_t *ui);
static void trend_popup_show_shell(ui_home_t *ui);
static void trend_popup_build_panel(ui_home_t *ui);
static void trend_overlay_destroy(void);

static void trend_log_mem(const char *step)
{
    ESP_LOGI(TAG, "[trend] %s | heap free=%u min_free=%u",
             step,
             (unsigned)esp_get_free_heap_size(),
             (unsigned)esp_get_minimum_free_heap_size());
}

static const lv_font_t *trend_font_body(void)
{
    return &ui_font_home_22;
}

static void trend_style_option_btn(lv_obj_t *btn)
{
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1E3050), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(btn, lv_color_hex(0x0F1A28), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x3BCFFF), LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);
}

static void home_login_stage_timer_cancel(void)
{
    if (s_home_login_stage_timer) {
        lv_timer_delete(s_home_login_stage_timer);
        s_home_login_stage_timer = NULL;
    }
}

static void home_login_demo_resume_timer_cancel(void)
{
    if (s_home_login_demo_resume_timer) {
        lv_timer_delete(s_home_login_demo_resume_timer);
        s_home_login_demo_resume_timer = NULL;
    }
}

static void home_screen_set_solid_bg(ui_home_t *ui, bool solid)
{
    if (!ui || !ui->screen) {
        return;
    }
    if (solid) {
        lv_obj_set_style_bg_color(ui->screen, lv_color_hex(0x0A1528), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(ui->screen, LV_OPA_COVER, LV_PART_MAIN);
    } else {
        lv_obj_set_style_bg_opa(ui->screen, LV_OPA_TRANSP, LV_PART_MAIN);
    }
}

static void home_screen_set_children_hidden(ui_home_t *ui, bool hidden, bool hide_bg)
{
    if (!ui || !ui->screen) {
        return;
    }

    const uint32_t cnt = lv_obj_get_child_count(ui->screen);
    for (uint32_t i = 0; i < cnt; i++) {
        lv_obj_t *child = lv_obj_get_child(ui->screen, i);
        if (!child) {
            continue;
        }
        if (!hide_bg && child == ui->bg_img) {
            continue;
        }
        lv_obj_set_hidden(child, hidden);
    }
}

static void home_screen_show_only_bg(ui_home_t *ui)
{
    if (!ui || !ui->screen) {
        return;
    }

    const uint32_t cnt = lv_obj_get_child_count(ui->screen);
    for (uint32_t i = 0; i < cnt; i++) {
        lv_obj_t *child = lv_obj_get_child(ui->screen, i);
        if (!child) {
            continue;
        }
        lv_obj_set_hidden(child, child != ui->bg_img);
    }
    home_screen_set_solid_bg(ui, false);
}

static void home_login_stage_resume_demo(ui_home_t *ui);

static void home_login_stage_unhide_widgets(ui_home_t *ui)
{
    if (!ui || !ui->screen) {
        return;
    }

    const uint32_t cnt = lv_obj_get_child_count(ui->screen);
    for (uint32_t i = 0; i < cnt; i++) {
        lv_obj_t *child = lv_obj_get_child(ui->screen, i);
        if (!child || child == ui->bg_img) {
            continue;
        }
        lv_obj_set_hidden(child, false);
    }
    home_screen_set_solid_bg(ui, false);
    ESP_LOGI(TAG, "[home] login staged: widgets on");
}

static void home_login_demo_resume_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    s_home_login_demo_resume_timer = NULL;

    ui_home_t *ui = s_home_login_staged_ui;
    if (!ui || !ui->screen || lv_screen_active() != ui->screen) {
        s_home_login_staged_ui = NULL;
        return;
    }
    home_login_stage_resume_demo(ui);
}

static void home_login_schedule_demo_resume(ui_home_t *ui)
{
    (void)ui;
    home_login_demo_resume_timer_cancel();
    s_home_login_demo_resume_timer =
        lv_timer_create(home_login_demo_resume_timer_cb, HOME_LOGIN_DEMO_RESUME_MS, NULL);
    if (s_home_login_demo_resume_timer) {
        lv_timer_set_repeat_count(s_home_login_demo_resume_timer, 1);
        ESP_LOGI(TAG, "[home] login staged: demo resume in %d ms", HOME_LOGIN_DEMO_RESUME_MS);
    } else {
        home_login_stage_resume_demo(s_home_login_staged_ui);
    }
}

static void home_login_stage_resume_demo(ui_home_t *ui)
{
    s_home_login_entry_complete = true;
    if (ui && ui->demo_timer) {
        lv_timer_resume(ui->demo_timer);
    }
    s_home_login_staged_ui = NULL;
    ESP_LOGI(TAG, "[home] login staged entry done");
}

static void home_login_stage_timer_cb(lv_timer_t *timer);

static void home_login_stage_schedule(uint32_t delay_ms)
{
    home_login_stage_timer_cancel();
    s_home_login_stage_timer = lv_timer_create(home_login_stage_timer_cb, delay_ms, NULL);
    if (s_home_login_stage_timer) {
        lv_timer_set_repeat_count(s_home_login_stage_timer, 1);
    }
}

static void home_login_stage_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    s_home_login_stage_timer = NULL;

    ui_home_t *ui = s_home_login_staged_ui;

    if (!ui || !ui->screen || lv_screen_active() != ui->screen) {
        s_home_login_staged_ui = NULL;
        return;
    }

    if (s_home_login_stage_phase == 0) {
        s_home_login_stage_phase = 1;
        home_screen_show_only_bg(ui);
        ESP_LOGI(TAG, "[home] login staged: bg only (widgets in %d ms)", HOME_LOGIN_WIDGETS_STEP_MS);
        home_login_stage_schedule(HOME_LOGIN_WIDGETS_STEP_MS);
        return;
    }

    if (s_home_login_stage_phase == 1) {
        s_home_login_stage_phase = 2;
        home_login_stage_unhide_widgets(ui);
        lv_obj_update_layout(ui->screen);
        home_login_schedule_demo_resume(ui);
    }
}

bool ui_home_events_login_staged_armed(void)
{
    return s_home_login_staged_armed;
}

void ui_home_events_arm_login_staged(ui_home_t *ui)
{
    (void)ui;
    /* 保留 API；登录进主页已改为与背景同帧显示，见 ui_goto_home_after_login */
}

static void trend_home_screen_slim(ui_home_t *ui, bool slim)
{
    if (!ui || !ui->screen) {
        return;
    }

    const uint32_t cnt = lv_obj_get_child_count(ui->screen);
    for (uint32_t i = 0; i < cnt; i++) {
        lv_obj_t *child = lv_obj_get_child(ui->screen, i);
        if (!child || child == ui->bg_img) {
            continue;
        }
        lv_obj_set_hidden(child, slim);
    }

    if (slim) {
        lv_obj_invalidate(ui->screen);
    }
}

static void trend_nav_timer_cancel(void)
{
    if (s_trend_nav_timer) {
        lv_timer_delete(s_trend_nav_timer);
        s_trend_nav_timer = NULL;
    }
}

static void trend_overlay_destroy(void)
{
    if (s_trend_overlay) {
        lv_obj_delete(s_trend_overlay);
        s_trend_overlay = NULL;
    }
}

static void trend_overlay_dismiss_only(void)
{
    trend_overlay_destroy();
}

static void trend_overlay_close_sync(bool resume_home_demo)
{
    if (!s_trend_overlay) {
        ESP_LOGW(TAG, "[trend] overlay_close: already closed");
        return;
    }

    trend_log_mem("overlay_close begin");
    trend_overlay_dismiss_only();

    if (s_trend_home_ui) {
        if (resume_home_demo) {
            trend_home_screen_slim(s_trend_home_ui, false);
            if (s_trend_home_ui->demo_timer) {
                lv_timer_resume(s_trend_home_ui->demo_timer);
            }
        }
        s_trend_home_ui = NULL;
    }
    trend_log_mem("overlay_close done");
}

static void trend_overlay_close_deferred_cb(lv_timer_t *timer)
{
    ESP_LOGI(TAG, "[trend] close deferred timer");
    s_trend_nav_timer = NULL;
    lv_timer_delete(timer);
    trend_overlay_close_sync(true);
}

static void trend_nav_to_curve_cb(lv_timer_t *timer)
{
    ESP_LOGI(TAG, "[trend] nav to curve ch=%d", (int)s_trend_nav_channel);
    s_trend_nav_timer = NULL;
    lv_timer_delete(timer);
    trend_log_mem("nav to curve begin");
    ui_trend_open(s_trend_nav_channel);
    trend_overlay_dismiss_only();
    if (s_trend_home_ui) {
        s_trend_home_ui = NULL;
    }
    trend_log_mem("nav to curve done");
}

static void trend_overlay_close_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ESP_LOGI(TAG, "[trend] close button clicked");
    trend_nav_timer_cancel();
    s_trend_nav_timer = lv_timer_create(trend_overlay_close_deferred_cb, 1, NULL);
    if (s_trend_nav_timer) {
        lv_timer_set_repeat_count(s_trend_nav_timer, 1);
    } else {
        trend_overlay_close_sync(true);
    }
}

static void trend_option_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    const intptr_t idx = (intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= (intptr_t)UI_TREND_CHANNEL_COUNT) {
        return;
    }

    ESP_LOGI(TAG, "[trend] option clicked: %s (idx=%d)", s_trend_opts[idx], (int)idx);
    trend_log_mem("option clicked");
    s_trend_nav_channel = (ui_trend_channel_t)idx;
    trend_nav_timer_cancel();
    s_trend_nav_timer = lv_timer_create(trend_nav_to_curve_cb, 2, NULL);
    if (s_trend_nav_timer) {
        lv_timer_set_repeat_count(s_trend_nav_timer, 1);
        ESP_LOGI(TAG, "[trend] scheduled nav to curve");
    } else {
        ui_trend_open(s_trend_nav_channel);
        trend_overlay_dismiss_only();
        s_trend_home_ui = NULL;
    }
}

static void trend_popup_show_deferred_cb(lv_timer_t *timer)
{
    ui_home_t *ui = lv_timer_get_user_data(timer);
    s_trend_nav_timer = NULL;
    lv_timer_delete(timer);
    if (!ui || !ui->screen) {
        ESP_LOGE(TAG, "[trend] show aborted: ui/screen null");
        return;
    }
    if (s_trend_overlay) {
        ESP_LOGW(TAG, "[trend] show aborted: popup already open");
        return;
    }

    trend_popup_show(ui);
}

static void trend_popup_build_cb(lv_timer_t *timer)
{
    ui_home_t *ui = lv_timer_get_user_data(timer);
    s_trend_nav_timer = NULL;
    lv_timer_delete(timer);
    if (!ui || !ui->screen || !s_trend_overlay) {
        return;
    }
    trend_popup_build_panel(ui);
}

static void trend_popup_show_shell(ui_home_t *ui)
{
    if (!ui || !ui->screen || s_trend_overlay) {
        return;
    }

    s_trend_overlay = lv_obj_create(ui->screen);
    if (!s_trend_overlay) {
        ESP_LOGE(TAG, "[trend] overlay create failed");
        return;
    }

    lv_obj_set_size(s_trend_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_align(s_trend_overlay, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_pad_all(s_trend_overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_trend_overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_trend_overlay, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_trend_overlay, LV_OPA_50, LV_PART_MAIN);
    lv_obj_set_scrollable(s_trend_overlay, false);
    lv_obj_move_foreground(s_trend_overlay);

    ESP_LOGI(TAG, "[trend] overlay shell=%p on home screen", (void *)s_trend_overlay);
}

static void trend_popup_abort_prepare(ui_home_t *ui)
{
    trend_overlay_destroy();
    if (ui) {
        trend_home_screen_slim(ui, false);
        if (ui->demo_timer) {
            lv_timer_resume(ui->demo_timer);
        }
    }
    s_trend_home_ui = NULL;
}

static void trend_popup_show(ui_home_t *ui)
{
    if (!ui || !ui->screen || s_trend_overlay) {
        return;
    }

    trend_log_mem("popup_show begin");
    ESP_LOGI(TAG, "[trend] popup build %s", TREND_POPUP_BUILD_TAG);

    s_trend_home_ui = ui;
    trend_home_screen_slim(ui, true);
    if (ui->demo_timer) {
        lv_timer_pause(ui->demo_timer);
    }

    trend_popup_show_shell(ui);
    if (!s_trend_overlay) {
        trend_popup_abort_prepare(ui);
        return;
    }

    trend_nav_timer_cancel();
    s_trend_nav_timer = lv_timer_create(trend_popup_build_cb, 1, ui);
    if (s_trend_nav_timer) {
        lv_timer_set_repeat_count(s_trend_nav_timer, 1);
    } else {
        trend_popup_build_panel(ui);
    }
}

static void trend_popup_build_panel(ui_home_t *ui)
{
    if (!ui || !ui->screen || !s_trend_overlay) {
        return;
    }

    trend_log_mem("panel build begin");

    lv_obj_t *panel = lv_obj_create(s_trend_overlay);
    if (!panel) {
        ESP_LOGE(TAG, "[trend] panel create failed");
        trend_popup_abort_prepare(ui);
        return;
    }

    lv_obj_set_size(panel, TREND_PANEL_W, TREND_PANEL_H);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(panel, 14, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, 16, LV_PART_MAIN);
    lv_obj_set_style_pad_row(panel, 12, LV_PART_MAIN);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x0A1528), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x3BCFFF), LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 2, LV_PART_MAIN);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(panel, false);

    lv_obj_t *title_lbl = lv_label_create(panel);
    lv_label_set_text(title_lbl, "趋势查看");
    lv_obj_set_style_text_font(title_lbl, trend_font_body(), LV_PART_MAIN);
    lv_obj_set_style_text_color(title_lbl, lv_color_hex(0x5EDFFF), LV_PART_MAIN);

    lv_obj_t *hint = lv_label_create(panel);
    lv_label_set_text(hint, "请选择：");
    lv_obj_set_style_text_font(hint, trend_font_body(), LV_PART_MAIN);
    lv_obj_set_style_text_color(hint, lv_color_hex(0xF0F8FF), LV_PART_MAIN);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    for (size_t i = 0; i < sizeof(s_trend_opts) / sizeof(s_trend_opts[0]); i++) {
        lv_obj_t *btn = lv_button_create(panel);
        if (!btn) {
            trend_overlay_close_sync(true);
            return;
        }
        lv_obj_set_size(btn, 320, 52);
        trend_style_option_btn(btn);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, s_trend_opts[i]);
        lv_obj_set_style_text_font(lbl, trend_font_body(), LV_PART_MAIN);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xF0F8FF), LV_PART_MAIN);
        lv_obj_center(lbl);

        lv_obj_add_event_cb(btn, trend_option_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    lv_obj_t *btn_close = lv_button_create(panel);
    lv_obj_set_size(btn_close, 200, 48);
    lv_obj_set_style_radius(btn_close, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn_close, lv_color_hex(0x1E3050), LV_PART_MAIN);
    lv_obj_add_event_cb(btn_close, trend_overlay_close_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_close = lv_label_create(btn_close);
    lv_label_set_text(lbl_close, "关闭");
    lv_obj_set_style_text_font(lbl_close, trend_font_body(), LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_close, lv_color_hex(0xF0F8FF), LV_PART_MAIN);
    lv_obj_center(lbl_close);

    trend_log_mem("panel build done");
}

void ui_home_events_dismiss_popups(ui_home_t *ui)
{
    trend_nav_timer_cancel();
    if (s_trend_overlay) {
        trend_overlay_close_sync(false);
    }
    if (ui && ui->screen) {
        trend_home_screen_slim(ui, false);
    }
}

void ui_home_events_on_screen_leave(ui_home_t *ui)
{
    home_login_stage_timer_cancel();
    home_login_demo_resume_timer_cancel();
    s_home_login_staged_armed = false;
    s_home_login_staged_ui = NULL;
    s_home_login_stage_phase = 0;
    s_home_login_entry_complete = false;

    if (!ui || !ui->screen) {
        return;
    }

    ui_home_events_dismiss_popups(ui);

    if (ui->demo_timer) {
        lv_timer_pause(ui->demo_timer);
    }
    home_screen_set_children_hidden(ui, false, false);
    home_screen_set_solid_bg(ui, false);
}

void ui_home_events_on_screen_active(ui_home_t *ui)
{
    if (!ui) {
        return;
    }

    home_login_stage_timer_cancel();
    home_login_demo_resume_timer_cancel();
    s_home_login_staged_armed = false;
    s_home_login_staged_ui = NULL;
    s_home_login_stage_phase = 0;

    home_screen_set_children_hidden(ui, false, false);
    home_screen_set_solid_bg(ui, false);

    s_home_login_entry_complete = true;
    trend_home_screen_slim(ui, false);
    if (ui->demo_timer) {
        lv_timer_resume(ui->demo_timer);
    }

    ui_home_refresh_wifi_icon(ui);
}

static void home_btn_alarm_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_home_t *ui = lv_event_get_user_data(e);
    if (ui) {
        ui_home_events_dismiss_popups(ui);
    }
    ui_change_screen(UI_SCR_ui_alarm);
}

static void home_btn_mode_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ESP_LOGI(TAG, "open run mode screen");
    ui_change_screen(UI_SCR_ui_mode);
}

static void home_btn_wifi_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_home_t *ui = lv_event_get_user_data(e);
    if (ui) {
        ui_home_events_dismiss_popups(ui);
    }
    ESP_LOGI(TAG, "open ui_wifi");
    ui_change_screen(UI_SCR_ui_wifi);
}

static void home_btn_settings_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_home_t *ui = lv_event_get_user_data(e);
    if (ui) {
        ui_home_events_dismiss_popups(ui);
    }
    ui_change_screen(UI_SCR_ui_param);
}

static void home_btn_trend_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    ui_home_t *ui = lv_event_get_user_data(e);
    if (!ui) {
        return;
    }
    if (s_trend_overlay) {
        return;
    }
    trend_nav_timer_cancel();
    s_trend_nav_timer = lv_timer_create(trend_popup_show_deferred_cb, 1, ui);
    if (s_trend_nav_timer) {
        lv_timer_set_repeat_count(s_trend_nav_timer, 1);
    } else {
        trend_popup_show(ui);
    }
}

static void status_text_green(lv_obj_t *label)
{
    lv_obj_set_style_text_color(label, lv_color_hex(0x3DDC84), LV_PART_MAIN);
}

static void status_text_orange(lv_obj_t *label)
{
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFB74D), LV_PART_MAIN);
}

static void home_btn_logout_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ESP_LOGI(TAG, "logout -> ui_login");
    ui_change_screen(UI_SCR_ui_login);
}

static void home_btn_start_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_home_t *ui = lv_event_get_user_data(e);
    if (!ui) {
        return;
    }
    ESP_LOGI(TAG, "start");
    if (ui->lbl_dev_fan) {
        lv_label_set_text(ui->lbl_dev_fan, "运行");
        status_text_green(ui->lbl_dev_fan);
    }
}

static void home_btn_stop_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_home_t *ui = lv_event_get_user_data(e);
    if (!ui) {
        return;
    }
    ESP_LOGI(TAG, "stop");
    if (ui->lbl_dev_fan) {
        lv_label_set_text(ui->lbl_dev_fan, "停止");
        status_text_orange(ui->lbl_dev_fan);
    }
}

static void home_demo_timer_cb(lv_timer_t *timer)
{
    ui_home_t *ui = lv_timer_get_user_data(timer);
    if (!ui || !ui->screen || lv_screen_active() != ui->screen) {
        return;
    }
    if (s_trend_overlay) {
        return;
    }
    if (!s_home_login_entry_complete) {
        return;
    }

    char buf[16];
    if(co2_struct.En == false)
    {
        snprintf(buf, sizeof(buf), "%.1f", 0.0);
        lv_label_set_text(ui->lbl_co2_val, buf);
        lv_arc_set_value(ui->arc_co2, 0);

        if (ui->scale_co2_needle && ui->needle_co2) 
            lv_scale_set_line_needle_value(ui->scale_co2_needle, ui->needle_co2, 62, 0);
    }
    else
    {
        snprintf(buf, sizeof(buf), "%.1f", (double)co2_struct.CO2_Real_Value);
        lv_label_set_text(ui->lbl_co2_val, buf);
        lv_arc_set_value(ui->arc_co2, (int32_t)co2_struct.CO2_Real_Value);

        if (ui->scale_co2_needle && ui->needle_co2) 
            lv_scale_set_line_needle_value(ui->scale_co2_needle, ui->needle_co2, 62, (int32_t)co2_struct.CO2_Real_Value);
    }

    if(temp_struct.En == false)
    {
        snprintf(buf, sizeof(buf), "%.1f", 0.0);
        lv_label_set_text(ui->lbl_temp_val, buf);
        lv_arc_set_value(ui->arc_temp, 0);

        if (ui->scale_temp_needle && ui->needle_temp) 
            lv_scale_set_line_needle_value(ui->scale_temp_needle, ui->needle_temp, 62, 0);
    }
    else
    {
        snprintf(buf, sizeof(buf), "%.1f", (double)temp_struct.Temp_Real_Value);
        lv_label_set_text(ui->lbl_temp_val, buf);
        lv_arc_set_value(ui->arc_temp, (int32_t)temp_struct.Temp_Real_Value);

        if (ui->scale_temp_needle && ui->needle_temp) 
            lv_scale_set_line_needle_value(ui->scale_temp_needle, ui->needle_temp, 62, (int32_t)temp_struct.Temp_Real_Value);
    }

    if (hum_struct.En == false) {
        snprintf(buf, sizeof(buf), "%.1f", 0.0);
        if (ui->lbl_hum_val) {
            lv_label_set_text(ui->lbl_hum_val, buf);
        }
        if (ui->arc_hum) {
            lv_arc_set_value(ui->arc_hum, 0);
        }
        if (ui->scale_hum_needle && ui->needle_hum) {
            lv_scale_set_line_needle_value(ui->scale_hum_needle, ui->needle_hum, 62, 0);
        }
    } else {
        snprintf(buf, sizeof(buf), "%.1f", (double)hum_struct.Hum_Real_Value);
        if (ui->lbl_hum_val) {
            lv_label_set_text(ui->lbl_hum_val, buf);
        }
        if (ui->arc_hum) {
            lv_arc_set_value(ui->arc_hum, (int32_t)hum_struct.Hum_Real_Value);
        }
        if (ui->scale_hum_needle && ui->needle_hum) {
            lv_scale_set_line_needle_value(ui->scale_hum_needle, ui->needle_hum, 62,
                                           (int32_t)hum_struct.Hum_Real_Value);
        }
    }

    if (rpm_struct.En == false) {
        snprintf(buf, sizeof(buf), "%ld", 0L);
        if (ui->lbl_rpm_val) {
            lv_label_set_text(ui->lbl_rpm_val, buf);
        }
        if (ui->scale_rpm && ui->needle_rpm) {
            lv_scale_set_line_needle_value(ui->scale_rpm, ui->needle_rpm, UI_HOME_RPM_NEEDLE_LEN, 0);
        }
    } else {
        snprintf(buf, sizeof(buf), "%ld", (long)(int32_t)rpm_struct.RPM_Real_Value);
        if (ui->lbl_rpm_val) {
            lv_label_set_text(ui->lbl_rpm_val, buf);
        }
        if (ui->scale_rpm && ui->needle_rpm) {
            lv_scale_set_line_needle_value(ui->scale_rpm, ui->needle_rpm, UI_HOME_RPM_NEEDLE_LEN,
                                           (int32_t)rpm_struct.RPM_Real_Value);
        }
    }
}

void ui_home_events_register(ui_home_t *ui)
{
    if (!ui || !ui->screen) {
        return;
    }

    app_lvgl_bind_swipe(ui->screen, 1);

    if (ui->btn_logout) {
        lv_obj_add_event_cb(ui->btn_logout, home_btn_logout_cb, LV_EVENT_CLICKED, NULL);
    }
    if (ui->btn_start) {
        lv_obj_add_event_cb(ui->btn_start, home_btn_start_cb, LV_EVENT_CLICKED, ui);
    }
    if (ui->btn_stop) {
        lv_obj_add_event_cb(ui->btn_stop, home_btn_stop_cb, LV_EVENT_CLICKED, ui);
    }
    if (ui->btn_mode) {
        lv_obj_add_event_cb(ui->btn_mode, home_btn_mode_cb, LV_EVENT_CLICKED, ui);
    }
    if (ui->btn_trend) {
        lv_obj_add_event_cb(ui->btn_trend, home_btn_trend_cb, LV_EVENT_CLICKED, ui);
    }
    if (ui->btn_settings) {
        lv_obj_add_event_cb(ui->btn_settings, home_btn_settings_cb, LV_EVENT_CLICKED, ui);
    }
    if (ui->btn_alarm) {
        lv_obj_add_event_cb(ui->btn_alarm, home_btn_alarm_cb, LV_EVENT_CLICKED, ui);
    }
    if (ui->img_wifi) {
        lv_obj_add_event_cb(ui->img_wifi, home_btn_wifi_cb, LV_EVENT_CLICKED, ui);
    }

    if (!ui->demo_timer) {
        ui->demo_timer = lv_timer_create(home_demo_timer_cb, HOME_DEMO_PERIOD_MS, ui);
    }
}

void ui_home_events_unregister(ui_home_t *ui)
{
    if (!ui) {
        return;
    }
    trend_nav_timer_cancel();
    home_login_stage_timer_cancel();
    home_login_demo_resume_timer_cancel();
    s_home_login_staged_armed = false;
    s_home_login_staged_ui = NULL;
    trend_overlay_close_sync(true);
    if (ui->demo_timer) {
        lv_timer_delete(ui->demo_timer);
        ui->demo_timer = NULL;
    }
}

void btn_logout_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED) 
        ui_change_screen(UI_SCR_ui_login);
}
