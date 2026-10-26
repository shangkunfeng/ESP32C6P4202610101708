#include "ui_login_events.h"

#include "app_lvgl.h"
#include "ui_common.h"

#include "esp_log.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

static const char *TAG = "ui_login_evt";

LV_FONT_DECLARE(ui_font_login_22);

/** 图表演示刷新周期（ms） */
#define CHART_DEMO_PERIOD_MS 700

#define LOGIN_TIP_PANEL_W 560
#define LOGIN_TIP_PANEL_H 280
#define LOGIN_TIP_DEFER_MS    16

/** 回到登录屏后稍等再允许进 Home（避免与注销全刷、Home 分阶段收尾叠帧） */
#define LOGIN_NAV_MIN_MS_AFTER_ACTIVE 200
#define LOGIN_CHART_RESUME_MS         180

/** 登录提示：本屏 overlay，不用 lv_msgbox / lv_layer_top（避免 FULL 刷新崩溃） */
static lv_obj_t *s_login_tip_overlay;
static lv_timer_t *s_login_tip_defer_timer;
static lv_timer_t *s_login_tip_close_timer;
static lv_timer_t *s_login_nav_home_timer;
static lv_timer_t *s_login_chart_resume_timer;
static uint32_t s_login_screen_active_tick;
static ui_login_t *s_login_tip_ui;

typedef struct {
    ui_login_t *ui;
    const char *title;
    const char *text;
} login_tip_defer_t;

static login_tip_defer_t s_login_tip_defer;

/** 登录屏：初始 / 错误(仅填一项) / 正常(两项已填) */
typedef enum {
    LOGIN_FSM_INITIAL,
    LOGIN_FSM_ERROR,
    LOGIN_FSM_NORMAL,
} login_fsm_state_t;

static login_fsm_state_t s_login_fsm = LOGIN_FSM_INITIAL;

static void login_keyboard_hide(ui_login_t *ui);
static void login_fsm_sync(ui_login_t *ui);
static const char *login_fsm_name(login_fsm_state_t state);
static void login_tip_pause_charts(ui_login_t *ui, bool pause);
static void login_tip_cancel_timers(void);
static void login_tip_overlay_destroy(void);
static void login_tip_close_sync(void);
static void login_prepare_leave_for_home(ui_login_t *ui);
static void login_tip_show_now(ui_login_t *ui, const char *title, const char *text);
static void login_chart_resume_timer_cancel(void);
static uint32_t login_nav_home_delay_ms(void);

static bool login_field_filled(const char *text)
{
    return text && text[0] != '\0';
}

static login_fsm_state_t login_fsm_derive(const char *user, const char *pass)
{
    const bool has_user = login_field_filled(user);
    const bool has_pass = login_field_filled(pass);

    if (!has_user && !has_pass) {
        return LOGIN_FSM_INITIAL;
    }
    if (has_user && has_pass) {
        return LOGIN_FSM_NORMAL;
    }
    return LOGIN_FSM_ERROR;
}

static const char *login_fsm_name(login_fsm_state_t state)
{
    switch (state) {
    case LOGIN_FSM_INITIAL:
        return "initial";
    case LOGIN_FSM_ERROR:
        return "error";
    case LOGIN_FSM_NORMAL:
        return "normal";
    default:
        return "?";
    }
}

/** 根据当前输入框内容刷新状态（进入登录屏、输入变化时调用） */
static void login_fsm_sync(ui_login_t *ui)
{
    const char *user = (ui && ui->ta_user) ? lv_textarea_get_text(ui->ta_user) : "";
    const char *pass = (ui && ui->ta_pass) ? lv_textarea_get_text(ui->ta_pass) : "";
    s_login_fsm = login_fsm_derive(user, pass);
}

static void login_fsm_sync_on_enter(ui_login_t *ui)
{
    login_fsm_sync(ui);
    if (s_login_fsm == LOGIN_FSM_NORMAL) {
        ESP_LOGI(TAG, "login enter: user+pass present -> fsm=normal");
    }
}

static void login_chart_resume_timer_cancel(void)
{
    if (s_login_chart_resume_timer) {
        lv_timer_delete(s_login_chart_resume_timer);
        s_login_chart_resume_timer = NULL;
    }
}

static void login_chart_resume_timer_cb(lv_timer_t *timer)
{
    ui_login_t *ui = lv_timer_get_user_data(timer);
    s_login_chart_resume_timer = NULL;
    if (!ui || !ui->screen || lv_screen_active() != ui->screen) {
        return;
    }
    if (s_login_nav_home_timer) {
        return;
    }
    login_tip_pause_charts(ui, false);
}

static uint32_t login_nav_home_delay_ms(void)
{
    const uint32_t elapsed = lv_tick_elaps(s_login_screen_active_tick);
    if (elapsed >= LOGIN_NAV_MIN_MS_AFTER_ACTIVE) {
        return 32;
    }
    return LOGIN_NAV_MIN_MS_AFTER_ACTIVE - elapsed + 32;
}

static void login_tip_cancel_timers(void)
{
    if (s_login_tip_defer_timer) {
        lv_timer_delete(s_login_tip_defer_timer);
        s_login_tip_defer_timer = NULL;
    }
    s_login_tip_defer.ui = NULL;
    s_login_tip_defer.title = NULL;
    s_login_tip_defer.text = NULL;

    if (s_login_tip_close_timer) {
        lv_timer_delete(s_login_tip_close_timer);
        s_login_tip_close_timer = NULL;
    }
}

static void login_prepare_leave_for_home(ui_login_t *ui)
{
    login_tip_cancel_timers();
    login_chart_resume_timer_cancel();
    login_tip_overlay_destroy();
    s_login_tip_ui = NULL;
    login_keyboard_hide(ui);
    login_tip_pause_charts(ui, true);
}

static void login_tip_overlay_destroy(void)
{
    if (s_login_tip_overlay) {
        lv_obj_delete(s_login_tip_overlay);
        s_login_tip_overlay = NULL;
    }
}

static void login_tip_pause_charts(ui_login_t *ui, bool pause)
{
    if (!ui || !ui->chart_demo_timer) {
        return;
    }
    if (pause) {
        lv_timer_pause(ui->chart_demo_timer);
    } else {
        lv_timer_resume(ui->chart_demo_timer);
    }
}

static void login_tip_close_sync(void)
{
    ui_login_t *ui = s_login_tip_ui;
    login_tip_overlay_destroy();
    s_login_tip_ui = NULL;
    if (ui) {
        login_tip_pause_charts(ui, false);
    }
}

static void login_tip_close_deferred_cb(lv_timer_t *timer)
{
    s_login_tip_close_timer = NULL;
    lv_timer_delete(timer);
    login_tip_close_sync();
}

static void login_tip_close_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    if (s_login_tip_close_timer) {
        lv_timer_delete(s_login_tip_close_timer);
        s_login_tip_close_timer = NULL;
    }
    s_login_tip_close_timer = lv_timer_create(login_tip_close_deferred_cb, LOGIN_TIP_DEFER_MS, NULL);
    if (s_login_tip_close_timer) {
        lv_timer_set_repeat_count(s_login_tip_close_timer, 1);
    } else {
        login_tip_close_sync();
    }
}

static void login_tip_show_now(ui_login_t *ui, const char *title, const char *text)
{
    if (!ui || !ui->screen || !title || !text) {
        return;
    }

    if (lv_screen_active() != ui->screen) {
        ESP_LOGW(TAG, "login tip skip: not active screen");
        return;
    }

    login_tip_overlay_destroy();
    s_login_tip_ui = ui;
    login_keyboard_hide(ui);
    login_tip_pause_charts(ui, true);

    s_login_tip_overlay = lv_obj_create(ui->screen);
    if (!s_login_tip_overlay) {
        s_login_tip_ui = NULL;
        login_tip_pause_charts(ui, false);
        return;
    }

    lv_obj_set_size(s_login_tip_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_align(s_login_tip_overlay, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(s_login_tip_overlay, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_login_tip_overlay, LV_OPA_50, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_login_tip_overlay, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_login_tip_overlay, 0, LV_PART_MAIN);
    lv_obj_set_scrollable(s_login_tip_overlay, false);
    lv_obj_move_foreground(s_login_tip_overlay);

    lv_obj_t *panel = lv_obj_create(s_login_tip_overlay);
    lv_obj_set_size(panel, LOGIN_TIP_PANEL_W, LOGIN_TIP_PANEL_H);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x0B2A4A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x3BCFFF), LV_PART_MAIN);
    lv_obj_set_style_border_width(panel, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(panel, 14, LV_PART_MAIN);
    lv_obj_set_style_pad_all(panel, 20, LV_PART_MAIN);
    lv_obj_set_scrollable(panel, false);

    lv_obj_t *lbl_title = lv_label_create(panel);
    lv_label_set_text(lbl_title, title);
    lv_obj_set_style_text_font(lbl_title, &ui_font_login_22, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0x5EDFFF), LV_PART_MAIN);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 4);

    lv_obj_t *lbl_body = lv_label_create(panel);
    lv_label_set_text(lbl_body, text);
    lv_obj_set_width(lbl_body, LOGIN_TIP_PANEL_W - 48);
    lv_obj_set_style_text_font(lbl_body, &ui_font_login_22, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_body, lv_color_hex(0xF0F8FF), LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl_body, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(lbl_body, 8, LV_PART_MAIN);
    lv_obj_align(lbl_body, LV_ALIGN_CENTER, 0, -8);

    lv_obj_t *btn_ok = lv_button_create(panel);
    lv_obj_set_size(btn_ok, 200, 52);
    lv_obj_align(btn_ok, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_style_radius(btn_ok, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn_ok, lv_color_hex(0x1E6FD9), LV_PART_MAIN);
    lv_obj_add_event_cb(btn_ok, login_tip_close_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_ok = lv_label_create(btn_ok);
    lv_label_set_text(lbl_ok, "确定");
    lv_obj_set_style_text_font(lbl_ok, &ui_font_login_22, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_ok, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_center(lbl_ok);

    ESP_LOGI(TAG, "login tip overlay shown (defer=%dms)", LOGIN_TIP_DEFER_MS);
}

static void login_tip_defer_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (s_login_tip_defer_timer) {
        lv_timer_delete(s_login_tip_defer_timer);
        s_login_tip_defer_timer = NULL;
    }

    if (s_login_nav_home_timer) {
        ESP_LOGW(TAG, "login tip defer dropped: nav pending");
    } else if (s_login_tip_defer.ui && s_login_tip_defer.title && s_login_tip_defer.text) {
        login_tip_show_now(s_login_tip_defer.ui, s_login_tip_defer.title, s_login_tip_defer.text);
    }
    s_login_tip_defer.ui = NULL;
    s_login_tip_defer.title = NULL;
    s_login_tip_defer.text = NULL;
}

static void login_show_tip(ui_login_t *ui, const char *title, const char *text)
{
    if (!ui || !title || !text) {
        return;
    }

    login_tip_cancel_timers();

    s_login_tip_defer.ui = ui;
    s_login_tip_defer.title = title;
    s_login_tip_defer.text = text;
    s_login_tip_defer_timer = lv_timer_create(login_tip_defer_timer_cb, LOGIN_TIP_DEFER_MS, NULL);
    if (s_login_tip_defer_timer) {
        lv_timer_set_repeat_count(s_login_tip_defer_timer, 1);
    } else {
        login_tip_show_now(ui, title, text);
    }
}

static void login_keyboard_hide(ui_login_t *ui)
{
    if (!ui) {
        return;
    }

    if (ui->ta_user) {
        lv_obj_remove_state(ui->ta_user, LV_STATE_FOCUSED);
    }
    if (ui->ta_pass) {
        lv_obj_remove_state(ui->ta_pass, LV_STATE_FOCUSED);
    }

    if (ui->keyboard) {
        lv_keyboard_set_textarea(ui->keyboard, NULL);
        lv_obj_set_hidden(ui->keyboard, true);
    }
}

static void login_ta_event_cb(lv_event_t *e)
{
    ui_login_t *ui = lv_event_get_user_data(e);
    if (!ui || !ui->keyboard) {
        return;
    }

    lv_obj_t *ta = lv_event_get_target(e);
    const lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_VALUE_CHANGED) {
        login_fsm_sync(ui);
        return;
    }

    if (code == LV_EVENT_FOCUSED || code == LV_EVENT_CLICKED) {
        lv_keyboard_set_textarea(ui->keyboard, ta);
        lv_obj_set_hidden(ui->keyboard, false);
        lv_obj_move_foreground(ui->keyboard);
        return;
    }

    if (code == LV_EVENT_DEFOCUSED || code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        login_keyboard_hide(ui);
        login_fsm_sync(ui);
    }
}

static void login_nav_home_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (s_login_nav_home_timer) {
        lv_timer_delete(s_login_nav_home_timer);
        s_login_nav_home_timer = NULL;
    }
    ESP_LOGI(TAG, "login deferred -> ui_home (staged)");
    ui_goto_home_after_login();
}

static void login_btn_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    ui_login_t *ui = lv_event_get_user_data(e);
    if (!ui) {
        return;
    }

    if (s_login_nav_home_timer) {
        return;
    }

    login_fsm_sync(ui);

    if (s_login_fsm == LOGIN_FSM_INITIAL || s_login_fsm == LOGIN_FSM_ERROR) {
        login_show_tip(ui, "提示", "请输入用户名和密码");
        ESP_LOGW(TAG, "login rejected: fsm=%s", login_fsm_name(s_login_fsm));
        return;
    }

    const char *user = lv_textarea_get_text(ui->ta_user);
    const char *pass = lv_textarea_get_text(ui->ta_pass);

    ESP_LOGI(TAG, "login ok fsm=normal user=%s pass_len=%u", user,
             (unsigned)strlen(pass));
    login_prepare_leave_for_home(ui);

    if (s_login_nav_home_timer) {
        lv_timer_delete(s_login_nav_home_timer);
        s_login_nav_home_timer = NULL;
    }
    const uint32_t nav_delay = login_nav_home_delay_ms();
    s_login_nav_home_timer = lv_timer_create(login_nav_home_timer_cb, nav_delay, NULL);
    if (s_login_nav_home_timer) {
        lv_timer_set_repeat_count(s_login_nav_home_timer, 1);
        ESP_LOGI(TAG, "login nav home scheduled in %u ms", (unsigned)nav_delay);
    } else {
        ui_goto_home_after_login();
    }
}

static int32_t wave_value(uint16_t phase_deg, uint32_t index, int32_t mid, int32_t amp, float phase_offset)
{
    const float deg = (float)(phase_deg + (uint16_t)(index * 28U)) + phase_offset;
    const float rad = deg * (3.14159265f / 180.0f);
    int32_t v = mid + (int32_t)(amp * sinf(rad));
    if (v < 4) {
        v = 4;
    }
    if (v > 58) {
        v = 58;
    }
    return v;
}

static void chart_demo_timer_cb(lv_timer_t *timer)
{
    ui_login_t *ui = lv_timer_get_user_data(timer);
    if (!ui || !ui->chart_bar || !ui->chart_line || !ui->chart_bar_ser || !ui->chart_line_ser) {
        return;
    }

    if (ui->screen && lv_screen_active() != ui->screen) {
        return;
    }
    if (s_login_tip_overlay) {
        return;
    }

    ui->chart_demo_phase = (uint16_t)(ui->chart_demo_phase + 14U);

    const uint32_t n = ui->chart_point_cnt;
    for (uint32_t i = 0; i < n; i++) {
        const int32_t bar_v = wave_value(ui->chart_demo_phase, i, 34, 18, 0.0f);
        const int32_t line_v = wave_value(ui->chart_demo_phase, i, 32, 16, 55.0f);
        lv_chart_set_value_by_id(ui->chart_bar, ui->chart_bar_ser, i, bar_v);
        lv_chart_set_value_by_id(ui->chart_line, ui->chart_line_ser, i, line_v);
    }

    lv_chart_refresh(ui->chart_bar);
    lv_chart_refresh(ui->chart_line);
}

void ui_login_events_on_screen_active(ui_login_t *ui)
{
    if (!ui) {
        return;
    }
    login_tip_cancel_timers();
    login_chart_resume_timer_cancel();
    if (s_login_nav_home_timer) {
        lv_timer_delete(s_login_nav_home_timer);
        s_login_nav_home_timer = NULL;
    }
    login_tip_overlay_destroy();
    s_login_tip_ui = NULL;
    s_login_screen_active_tick = lv_tick_get();
    login_tip_pause_charts(ui, true);
    login_keyboard_hide(ui);
    login_fsm_sync_on_enter(ui);
    s_login_chart_resume_timer = lv_timer_create(login_chart_resume_timer_cb, LOGIN_CHART_RESUME_MS, ui);
    if (s_login_chart_resume_timer) {
        lv_timer_set_repeat_count(s_login_chart_resume_timer, 1);
    } else {
        login_tip_pause_charts(ui, false);
    }
    ESP_LOGI(TAG, "login screen active fsm=%s", login_fsm_name(s_login_fsm));
}

void ui_login_events_register(ui_login_t *ui)
{
    if (!ui || !ui->screen) {
        return;
    }

    app_lvgl_bind_swipe(ui->screen, 0);

    if (ui->btn_login) 
    {
        lv_obj_add_event_cb(ui->btn_login, login_btn_cb, LV_EVENT_CLICKED, ui);
    }

    if (ui->keyboard) 
    {
        static const lv_event_code_t ta_events[] = 
        {
            LV_EVENT_VALUE_CHANGED,
            LV_EVENT_FOCUSED,
            LV_EVENT_DEFOCUSED,
            LV_EVENT_CLICKED,
            LV_EVENT_READY,
            LV_EVENT_CANCEL,
        };

        if (ui->ta_user) 
        {
            for (size_t i = 0; i < sizeof(ta_events) / sizeof(ta_events[0]); i++) 
            {
                lv_obj_add_event_cb(ui->ta_user, login_ta_event_cb, ta_events[i], ui);
            }
        }

        if (ui->ta_pass) 
        {
            for (size_t i = 0; i < sizeof(ta_events) / sizeof(ta_events[0]); i++) 
            {
                lv_obj_add_event_cb(ui->ta_pass, login_ta_event_cb, ta_events[i], ui);
            }
        }
    }

    if (ui->chart_bar && ui->chart_line && !ui->chart_demo_timer) 
    {
        ui->chart_demo_timer = lv_timer_create(chart_demo_timer_cb, CHART_DEMO_PERIOD_MS, ui);
        
        if (ui->chart_demo_timer) 
        {
            lv_timer_set_repeat_count(ui->chart_demo_timer, -1);
            lv_timer_pause(ui->chart_demo_timer);
        }
    }

    login_fsm_sync_on_enter(ui);
}

void ui_login_events_unregister(ui_login_t *ui)
{
    if (!ui) {
        return;
    }

    login_keyboard_hide(ui);

    login_tip_cancel_timers();
    login_chart_resume_timer_cancel();
    if (s_login_nav_home_timer) {
        lv_timer_delete(s_login_nav_home_timer);
        s_login_nav_home_timer = NULL;
    }
    login_tip_close_sync();

    if (ui->chart_demo_timer) {
        lv_timer_delete(ui->chart_demo_timer);
        ui->chart_demo_timer = NULL;
    }
}
