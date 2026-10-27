#include "ui_common.h"

#include "app_run_mode.h"
#include "app_wifi.h"
#include "common.h"
#include "app_sd_background.h"
#include "display_config.h"
#include "ui_login.h"
#include "ui_login_events.h"
#include "ui_home.h"
#include "ui_home_events.h"
#include "ui_mode.h"
#include "ui_wifi.h"
#include "ui_trend.h"
#include "ui_alarm.h"
#include "ui_alarm_events.h"
#include "ui_param.h"
#include "ui_param_events.h"
#include "uart_proto.h"

#include "esp_log.h"
#include "esp_system.h"

#include <stdbool.h>
#include <stdint.h>

static const char *TAG = "ui_common";

static ui_screen_id_t s_current = UI_SCR_COUNT;

static const char *ui_screen_name(ui_screen_id_t id)
{
    switch (id) {
    case UI_SCR_ui_login:
        return "ui_login";
    case UI_SCR_ui_home:
        return "ui_home";
    case UI_SCR_ui_mode:
        return "ui_mode";
    case UI_SCR_ui_wifi:
        return "ui_wifi";
    case UI_SCR_ui_trend:
        return "ui_trend";
    case UI_SCR_ui_alarm:
        return "ui_alarm";
    case UI_SCR_ui_param:
        return "ui_param";
    default:
        return "?";
    }
}

static ui_login_t s_ui_login;
static ui_home_t s_ui_home;
static ui_mode_t s_ui_mode;
static ui_wifi_t s_ui_wifi;
static ui_trend_t s_ui_trend;
static ui_alarm_t s_ui_alarm;
static ui_param_t s_ui_param;

static lv_image_dsc_t s_login_bg_dsc;
static lv_image_dsc_t s_ui_home_back_dsc;
static lv_image_dsc_t s_ui_home_dsc;
static bool s_login_bg_dsc_ready;
static bool s_ui_home_back_dsc_ready;
static bool s_ui_home_dsc_ready;
static bool s_all_screens_created;

typedef struct {
    lv_obj_t *lbl_fps;
    lv_obj_t *lbl_cpu;
} ui_perf_hud_t;

static ui_perf_hud_t s_perf_hud[UI_SCR_COUNT];
static lv_timer_t *s_perf_ui_timer;

typedef void (*ui_screen_fn_t)(void);

LV_FONT_DECLARE(lv_font_montserrat_14);

static void perf_ui_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    common_perf_service();

    for (int i = 0; i < UI_SCR_COUNT; i++) {
        if (s_perf_hud[i].lbl_fps) {
            lv_label_set_text_fmt(s_perf_hud[i].lbl_fps, "FPS:%u", (unsigned)g_ui_fps);
        }
        if (s_perf_hud[i].lbl_cpu) {
            lv_label_set_text_fmt(s_perf_hud[i].lbl_cpu, "CPU:%u%%", (unsigned)g_ui_cpu_usage_pct);
        }
    }
}

static void style_perf_label(lv_obj_t *lbl)
{
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x5EDFFF), LV_PART_MAIN);
    lv_obj_set_style_text_opa(lbl, LV_OPA_COVER, LV_PART_MAIN);
}

void ui_common_attach_perf_overlay(lv_obj_t *screen, ui_screen_id_t id)
{
    if (!screen || id >= UI_SCR_COUNT) {
        return;
    }

    lv_obj_t *bar = lv_obj_create(screen);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_height(bar, LV_SIZE_CONTENT);
    lv_obj_set_width(bar, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_hor(bar, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(bar, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_column(bar, 16, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x081828), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_70, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(bar, lv_color_hex(0x2A8FD4), LV_PART_MAIN);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollable(bar, false);
    lv_obj_set_clickable(bar, false);

    s_perf_hud[id].lbl_fps = lv_label_create(bar);
    lv_label_set_text(s_perf_hud[id].lbl_fps, "FPS:0");
    style_perf_label(s_perf_hud[id].lbl_fps);

    s_perf_hud[id].lbl_cpu = lv_label_create(bar);
    lv_label_set_text(s_perf_hud[id].lbl_cpu, "CPU:0%");
    style_perf_label(s_perf_hud[id].lbl_cpu);

    lv_obj_move_foreground(bar);
}

static void fill_image_dsc(lv_image_dsc_t *d, const uint16_t *pixels)
{
    d->header.magic = LV_IMAGE_HEADER_MAGIC;
    d->header.cf = LV_COLOR_FORMAT_RGB565;
    d->header.flags = 0;
    d->header.w = LCD_H_RES;
    d->header.h = LCD_V_RES;
    d->header.stride = (uint32_t)LCD_H_RES * sizeof(uint16_t);
    d->data_size = (uint32_t)LCD_H_RES * (uint32_t)LCD_V_RES * sizeof(uint16_t);
    d->data = (const uint8_t *)pixels;
}

static void ensure_login_bg_dsc(void)
{
    if (s_login_bg_dsc_ready) {
        return;
    }

    const uint16_t *px = app_sd_background_get_ui_login_rgb565();
    if (!px) {
        return;
    }

    fill_image_dsc(&s_login_bg_dsc, px);
    s_login_bg_dsc_ready = true;
}

static void ui_login_screen_create(void)
{
    ensure_login_bg_dsc();
    if (s_ui_login.screen) {
        return;
    }
    ui_login_create(&s_ui_login, &s_login_bg_dsc);
}

static void ensure_ui_home_back_dsc(void)
{
    if (s_ui_home_back_dsc_ready) {
        return;
    }

    const uint16_t *px = app_sd_background_get_ui_home_back_rgb565();
    if (!px) {
        return;
    }

    fill_image_dsc(&s_ui_home_back_dsc, px);
    s_ui_home_back_dsc_ready = true;
}

static void ensure_ui_home_dsc(void)
{
    if (s_ui_home_dsc_ready) {
        return;
    }

    const uint16_t *px = app_sd_background_get_ui_home_rgb565();
    if (!px) {
        return;
    }

    fill_image_dsc(&s_ui_home_dsc, px);
    s_ui_home_dsc_ready = true;
}

static void ui_home_screen_create(void)
{
    ensure_ui_home_back_dsc();
    if (s_ui_home.screen) {
        return;
    }
    ui_home_create(&s_ui_home, &s_ui_home_back_dsc);
}

static void ui_mode_screen_create(void)
{
    ensure_ui_home_dsc();
    if (s_ui_mode.screen) {
        return;
    }
    ui_mode_create(&s_ui_mode, &s_ui_home_dsc);
}

static void ui_wifi_screen_create(void)
{
    ensure_ui_home_dsc();
    if (s_ui_wifi.screen) {
        return;
    }
    ui_wifi_create(&s_ui_wifi, &s_ui_home_dsc);
}

static void ui_trend_screen_create(void)
{
    ensure_ui_home_dsc();
    if (s_ui_trend.screen) {
        return;
    }
    ui_trend_create(&s_ui_trend, &s_ui_home_dsc);
}

static void ui_alarm_screen_create(void)
{
    ensure_ui_home_dsc();
    if (s_ui_alarm.screen) {
        return;
    }
    ui_alarm_create(&s_ui_alarm, &s_ui_home_dsc);
}

static void ui_param_screen_create(void)
{
    ensure_ui_home_dsc();
    if (s_ui_param.screen) {
        return;
    }
    ui_param_create(&s_ui_param, &s_ui_home_dsc);
}

static const ui_screen_fn_t s_create_fn[UI_SCR_COUNT] = {
    ui_login_screen_create,
    ui_home_screen_create,
    ui_mode_screen_create,
    ui_wifi_screen_create,
    ui_trend_screen_create,
    ui_alarm_screen_create,
    ui_param_screen_create,
};

static lv_obj_t **const s_root[UI_SCR_COUNT] = {
    &s_ui_login.screen,
    &s_ui_home.screen,
    &s_ui_mode.screen,
    &s_ui_wifi.screen,
    &s_ui_trend.screen,
    &s_ui_alarm.screen,
    &s_ui_param.screen,
};

static void ui_ensure_screen(ui_screen_id_t id)
{
    if (id >= UI_SCR_COUNT) {
        return;
    }
    if (s_root[id] && *s_root[id]) {
        return;
    }
    if (s_create_fn[id]) {
        s_create_fn[id]();
    }
}

/** 启动时一次性创建全部界面；运行期不再创建/销毁，只 lv_screen_load */
static void wifi_state_async_cb(void *param)
{
    (void)param;
    ui_home_refresh_wifi_icon(&s_ui_home);
    if (s_ui_wifi.screen) {
        ui_wifi_refresh(&s_ui_wifi);
    }
}

static void wifi_state_notify_cb(void *user_data)
{
    (void)user_data;
    lv_async_call(wifi_state_async_cb, NULL);
}

static void uart_real_async_cb(void *param)
{
    (void)param;
    if (ui_get_current_screen() == UI_SCR_ui_home) {
        ui_home_refresh_real_values(&s_ui_home);
    }
}

static void uart_proto_ui_notify_cb(void)
{
    lv_async_call(uart_real_async_cb, NULL);
}

static void ui_precreate_all_screens(void)
{
    if (s_all_screens_created) {
        return;
    }

    for (int i = 0; i < UI_SCR_COUNT; i++) {
        ui_ensure_screen((ui_screen_id_t)i);
        if (!s_root[i] || !*s_root[i]) {
            ESP_LOGE(TAG, "precreate failed: %s", ui_screen_name((ui_screen_id_t)i));
        } else {
            ui_common_attach_perf_overlay(*s_root[i], (ui_screen_id_t)i);
        }
    }

    if (!s_perf_ui_timer) {
        s_perf_ui_timer = lv_timer_create(perf_ui_timer_cb, 1000, NULL);
        if (s_perf_ui_timer) {
            lv_timer_set_repeat_count(s_perf_ui_timer, -1);
        }
    }

    s_all_screens_created = true;
    app_wifi_set_ui_notify(wifi_state_notify_cb, NULL);
    uart_proto_set_ui_notify(uart_proto_ui_notify_cb);
    ui_home_refresh_wifi_icon(&s_ui_home);
    ESP_LOGI(TAG, "all %d screens created (persistent until power-off)", UI_SCR_COUNT);
}

bool ui_all_screens_created(void)
{
    return s_all_screens_created;
}

void ui_home_sync_mode_label(void)
{
    if (!s_ui_home.lbl_mode) {
        return;
    }
    lv_label_set_text(s_ui_home.lbl_mode, app_run_mode_line_text());
}

ui_screen_id_t ui_get_current_screen(void)
{
    return s_current;
}

static void ui_change_screen_impl(ui_screen_id_t id, bool refr_now)
{
    if (id >= UI_SCR_COUNT) {
        ESP_LOGE(TAG, "change_screen invalid id=%d", (int)id);
        return;
    }

    ESP_LOGI(TAG, "change_screen %s (%d) from %s heap free=%u refr=%d",
             ui_screen_name(id),
             (int)id,
             s_current < UI_SCR_COUNT ? ui_screen_name(s_current) : "none",
             (unsigned)esp_get_free_heap_size(),
             refr_now ? 1 : 0);

    if (!s_all_screens_created) {
        ESP_LOGE(TAG, "change_screen %s: call ui_init() first (screens not pre-created)",
                 ui_screen_name(id));
        return;
    }

    if (!s_root[id] || !*s_root[id]) {
        ESP_LOGE(TAG, "change_screen %s: root screen null", ui_screen_name(id));
        return;
    }

    if (s_current == UI_SCR_ui_home && id != UI_SCR_ui_home) {
        ui_home_events_on_screen_leave(&s_ui_home);
    }
    if (s_current == UI_SCR_ui_param && id != UI_SCR_ui_param) {
        ui_param_events_on_screen_leave(&s_ui_param);
    }
    if (s_current == UI_SCR_ui_mode && id != UI_SCR_ui_mode) {
        ui_mode_on_screen_leave(&s_ui_mode);
    }
    if (s_current == UI_SCR_ui_wifi && id != UI_SCR_ui_wifi) {
        ui_wifi_on_screen_leave(&s_ui_wifi);
    }

    s_current = id;
    ESP_LOGI(TAG, "lv_screen_load %p", (void *)*s_root[id]);
    lv_screen_load(*s_root[id]);
    lv_obj_invalidate(*s_root[id]);
    if (id == UI_SCR_ui_login) {
        ui_login_events_on_screen_active(&s_ui_login);
    }
    if (id == UI_SCR_ui_home) {
        ui_home_sync_mode_label();
        ui_home_events_on_screen_active(&s_ui_home);
    }
    if (id == UI_SCR_ui_param) {
        ui_param_events_on_screen_active(&s_ui_param);
    }
    if (id == UI_SCR_ui_alarm) {
        ui_alarm_events_on_screen_active(&s_ui_alarm);
    }
    if (id == UI_SCR_ui_mode) {
        ui_mode_on_screen_active(&s_ui_mode);
        ui_mode_refresh(&s_ui_mode);
    }
    if (id == UI_SCR_ui_wifi) {
        ui_wifi_on_screen_active(&s_ui_wifi);
    }
    if (refr_now) {
        lv_refr_now(NULL);
    }
    ESP_LOGI(TAG, "change_screen %s done active=%p",
             ui_screen_name(id), (void *)lv_screen_active());
}

void ui_change_screen(ui_screen_id_t id)
{
    ui_change_screen_impl(id, true);
}

void ui_change_screen_no_refr(ui_screen_id_t id)
{
    ui_change_screen_impl(id, false);
}

void ui_goto_home_after_login(void)
{
    ui_change_screen(UI_SCR_ui_home);
}

void ui_load_screen(lv_obj_t *scr)
{
    if (scr) {
        lv_screen_load(scr);
    }
}

void ui_init(void)
{
    ui_precreate_all_screens();
    ui_change_screen_no_refr(UI_SCR_ui_login);
}
