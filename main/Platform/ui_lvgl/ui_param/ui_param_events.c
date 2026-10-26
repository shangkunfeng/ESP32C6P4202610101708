#include "ui_param_events.h"

#include "app_lvgl.h"
#include "app_param_config.h"
#include "board_display.h"
#include "ui_common.h"

#include "esp_log.h"

#include <string.h>

#include "esp_err.h"

static const char *TAG = "ui_param_evt";

static ui_param_t *s_ui;

static void param_ta_sanitize_digits(lv_obj_t *ta)
{
    if (!ta) {
        return;
    }
    const char *cur = lv_textarea_get_text(ta);
    char num[24];
    size_t i = 0;
    if (!cur) {
        num[0] = '\0';
    } else {
        for (size_t k = 0; cur[k] && i + 1 < sizeof(num); k++) {
            const char c = cur[k];
            if (c >= '0' && c <= '9') {
                num[i++] = c;
            } else if (c == '.' && !strchr(num, '.')) {
                num[i++] = c;
            }
        }
        num[i] = '\0';
    }
    lv_textarea_set_text(ta, num);
}

static void param_keyboard_extra_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED || !s_ui || !s_ui->keyboard) {
        return;
    }

    lv_obj_t *kb = lv_event_get_target(e);
    if (kb != s_ui->keyboard) {
        return;
    }

    const uint32_t btn_id = lv_buttonmatrix_get_selected_button(kb);
    if (btn_id == LV_BUTTONMATRIX_BUTTON_NONE) {
        return;
    }

    const char *txt = lv_buttonmatrix_get_button_text(kb, btn_id);
    lv_obj_t *ta = lv_keyboard_get_textarea(kb);
    if (!txt || !ta) {
        return;
    }

    if (strcmp(txt, "清除") == 0) {
        lv_textarea_set_text(ta, "");
        return;
    }

    if (strcmp(txt, "删除") == 0) {
        param_ta_sanitize_digits(ta);
        lv_textarea_delete_char(ta);
        return;
    }

    if (strcmp(txt, "确认") == 0) {
        param_ta_sanitize_digits(ta);
        ui_param_clamp_textarea(ta);
        lv_obj_send_event(ta, LV_EVENT_READY, NULL);
    }
}

static void param_sanitize_panel(lv_obj_t *panel)
{
    if (!panel) {
        return;
    }

    const uint32_t n = lv_obj_get_child_count(panel);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *ch = lv_obj_get_child(panel, i);
        if (lv_obj_check_type(ch, &lv_textarea_class)) {
            param_ta_sanitize_digits(ch);
        }
    }
}

static void param_sanitize_all_panels(void)
{
    if (!s_ui) {
        return;
    }

    for (int i = 0; i < UI_PARAM_GROUP_COUNT; i++) {
        param_sanitize_panel(s_ui->group_panels[i]);
    }
}

static void param_keyboard_hide(void)
{
    if (!s_ui || !s_ui->keyboard) {
        return;
    }
    lv_keyboard_set_textarea(s_ui->keyboard, NULL);
    lv_obj_set_hidden(s_ui->keyboard, true);
}

static void param_keyboard_show_for(lv_obj_t *ta)
{
    if (!s_ui || !s_ui->keyboard || !ta) {
        return;
    }

    lv_obj_t *kb = s_ui->keyboard;
    ui_param_keyboard_place_near_field(kb, ta);
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_USER_1);

    if (lv_keyboard_get_textarea(kb) != ta) {
        lv_keyboard_set_textarea(kb, ta);
    } else {
        lv_obj_add_state(ta, LV_STATE_FOCUSED);
    }

    lv_obj_set_hidden(kb, false);
    lv_obj_move_foreground(kb);
    lv_obj_invalidate(kb);
}

static void param_value_ta_event_cb(lv_event_t *e)
{
    if (!s_ui || !s_ui->keyboard) {
        return;
    }

    lv_obj_t *ta = lv_event_get_target(e);
    const lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_FOCUSED || code == LV_EVENT_CLICKED) {
        param_keyboard_show_for(ta);
        return;
    }

    /* 不在 DEFOCUSED 时隐藏：否则 lv_keyboard_set_textarea 会立刻把键盘关掉 */
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        param_keyboard_hide();
    }
}

static void param_brightness_slider_cb(lv_event_t *e)
{
    if (!s_ui) {
        return;
    }

    const lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_VALUE_CHANGED && code != LV_EVENT_RELEASED) {
        return;
    }

    ui_param_sync_screen_brightness(s_ui, true);
}

void ui_param_events_attach_brightness_slider(ui_param_t *ui)
{
    if (!ui || !ui->slider_brightness) {
        return;
    }

    lv_obj_add_event_cb(ui->slider_brightness, param_brightness_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui->slider_brightness, param_brightness_slider_cb, LV_EVENT_RELEASED, NULL);
}

static void param_nav_btn_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED || !s_ui) {
        return;
    }

    ui_param_group_t group = (ui_param_group_t)(intptr_t)lv_event_get_user_data(e);
    if (group >= UI_PARAM_GROUP_COUNT) {
        return;
    }

    param_keyboard_hide();
    ui_param_show_page(s_ui, group);
}

void ui_param_events_attach_nav_button(lv_obj_t *btn, ui_param_group_t group)
{
    if (!btn || group >= UI_PARAM_GROUP_COUNT) {
        return;
    }

    lv_obj_add_event_cb(btn, param_nav_btn_cb, LV_EVENT_CLICKED,
                        (void *)(intptr_t)group);
}

void ui_param_events_attach_value(lv_obj_t *ta)
{
    if (!ta) {
        return;
    }

    static const lv_event_code_t codes[] = {
        LV_EVENT_FOCUSED,
        LV_EVENT_CLICKED,
        LV_EVENT_READY,
        LV_EVENT_CANCEL,
    };

    for (size_t i = 0; i < sizeof(codes) / sizeof(codes[0]); i++) {
        lv_obj_add_event_cb(ta, param_value_ta_event_cb, codes[i], NULL);
    }
}

static void param_btn_reset_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED || !s_ui) {
        return;
    }
    param_keyboard_hide();
    param_config_apply_factory_defaults();
    param_config_remove_user_file();
    ui_param_sync_from_drivers(s_ui);
    ESP_LOGI(TAG, "factory defaults applied");
}

static void param_btn_confirm_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    param_keyboard_hide();
    param_sanitize_all_panels();
    ui_param_clamp_all_fields(s_ui);
    ui_param_sync_to_drivers(s_ui);
    esp_err_t err = param_config_save_to_sd();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "save to SD failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "parameters saved to SD");
    }
}

static void param_btn_back_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    param_keyboard_hide();
    ui_change_screen(UI_SCR_ui_home);
}

void ui_param_events_on_screen_active(ui_param_t *ui)
{
    if (!ui || !ui->screen) {
        return;
    }
    s_ui = ui;
    ui_param_show_page(ui, ui->active_page);
    ui_param_sync_from_drivers(ui);
    ESP_LOGI(TAG, "ui_param screen active %p page=%d", (void *)ui->screen, (int)ui->active_page);
}

void ui_param_events_on_screen_leave(ui_param_t *ui)
{
    (void)ui;
    param_keyboard_hide();
}

void ui_param_events_register(ui_param_t *ui)
{
    if (!ui || !ui->screen) {
        return;
    }

    s_ui = ui;

    app_lvgl_bind_swipe(ui->screen, UI_SCR_ui_param);

    if (ui->btn_reset) {
        lv_obj_add_event_cb(ui->btn_reset, param_btn_reset_cb, LV_EVENT_CLICKED, NULL);
    }
    if (ui->btn_confirm) {
        lv_obj_add_event_cb(ui->btn_confirm, param_btn_confirm_cb, LV_EVENT_CLICKED, NULL);
    }
    if (ui->btn_back) {
        lv_obj_add_event_cb(ui->btn_back, param_btn_back_cb, LV_EVENT_CLICKED, NULL);
    }
    if (ui->keyboard) {
        lv_obj_add_event_cb(ui->keyboard, param_keyboard_extra_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }

    ui_param_events_attach_brightness_slider(ui);
}
