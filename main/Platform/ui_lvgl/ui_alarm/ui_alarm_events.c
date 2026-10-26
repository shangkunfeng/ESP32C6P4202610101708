#include "ui_alarm_events.h"

#include "app_lvgl.h"
#include "ui_common.h"
#include "ui_param.h"

#include "esp_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "ui_alarm_evt";

static ui_alarm_t *s_ui;

void ui_alarm_keyboard_setup(lv_obj_t *kb)
{
    static const char *const map[] = {
        "1", "2", "3", "\n",
        "4", "5", "6", "\n",
        "7", "8", "9", "\n",
        "清除", "0", "删除", "\n",
        "确认", "",
    };
    static const lv_buttonmatrix_ctrl_t ctrl[] = {
        LV_BUTTONMATRIX_CTRL_WIDTH_3, LV_BUTTONMATRIX_CTRL_WIDTH_3, LV_BUTTONMATRIX_CTRL_WIDTH_3,
        LV_BUTTONMATRIX_CTRL_WIDTH_3, LV_BUTTONMATRIX_CTRL_WIDTH_3, LV_BUTTONMATRIX_CTRL_WIDTH_3,
        LV_BUTTONMATRIX_CTRL_WIDTH_3, LV_BUTTONMATRIX_CTRL_WIDTH_3, LV_BUTTONMATRIX_CTRL_WIDTH_3,
        LV_BUTTONMATRIX_CTRL_WIDTH_6, LV_BUTTONMATRIX_CTRL_WIDTH_2, LV_BUTTONMATRIX_CTRL_WIDTH_2,
        LV_BUTTONMATRIX_CTRL_WIDTH_5,
    };

    lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_USER_1, map, ctrl);
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_USER_1);
}

static void alarm_jump_ta_sanitize(lv_obj_t *ta)
{
    if (!ta) {
        return;
    }
    const char *cur = lv_textarea_get_text(ta);
    char num[8];
    size_t i = 0;
    if (cur) {
        for (size_t k = 0; cur[k] && i + 1 < sizeof(num); k++) {
            if (cur[k] >= '0' && cur[k] <= '9') {
                num[i++] = cur[k];
            }
        }
    }
    num[i] = '\0';
    lv_textarea_set_text(ta, num);
}

static void alarm_keyboard_hide(void)
{
    if (!s_ui || !s_ui->keyboard) {
        return;
    }
    lv_keyboard_set_textarea(s_ui->keyboard, NULL);
    lv_obj_set_hidden(s_ui->keyboard, true);
}

static void alarm_keyboard_show_for(lv_obj_t *ta)
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
    lv_obj_move_foreground(s_ui->panel_jump);
    lv_obj_move_foreground(kb);
    lv_obj_invalidate(kb);
}

static void alarm_keyboard_extra_cb(lv_event_t *e)
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
        alarm_jump_ta_sanitize(ta);
        lv_textarea_delete_char(ta);
        return;
    }

    if (strcmp(txt, "确认") == 0) {
        alarm_jump_ta_sanitize(ta);
        lv_obj_send_event(ta, LV_EVENT_READY, NULL);
    }
}

static void alarm_jump_ta_event_cb(lv_event_t *e)
{
    if (!s_ui || !s_ui->keyboard) {
        return;
    }

    lv_obj_t *ta = lv_event_get_target(e);
    const lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_FOCUSED || code == LV_EVENT_CLICKED) {
        alarm_keyboard_show_for(ta);
        return;
    }

    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        alarm_keyboard_hide();
    }
}

static void alarm_btn_prev_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED || !s_ui) {
        return;
    }
    alarm_keyboard_hide();
    if (s_ui->current_page > 0) {
        s_ui->current_page--;
        ui_alarm_refresh_list(s_ui);
    }
}

static void alarm_btn_next_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED || !s_ui) {
        return;
    }
    alarm_keyboard_hide();
    if (s_ui->current_page + 1 < s_ui->page_count) {
        s_ui->current_page++;
        ui_alarm_refresh_list(s_ui);
    }
}

static void alarm_btn_jump_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED || !s_ui || !s_ui->panel_jump) {
        return;
    }
    lv_obj_set_hidden(s_ui->panel_jump, false);
    lv_obj_move_foreground(s_ui->panel_jump);
    if (s_ui->ta_jump_page) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", s_ui->current_page + 1);
        lv_textarea_set_text(s_ui->ta_jump_page, buf);
        alarm_keyboard_show_for(s_ui->ta_jump_page);
    }
}

static void alarm_btn_jump_ok_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED || !s_ui) {
        return;
    }
    if (!s_ui->ta_jump_page) {
        return;
    }
    alarm_jump_ta_sanitize(s_ui->ta_jump_page);
    const char *t = lv_textarea_get_text(s_ui->ta_jump_page);
    int page = t ? atoi(t) : 1;
    if (page < 1) {
        page = 1;
    }
    if (page > s_ui->page_count) {
        page = s_ui->page_count;
    }
    s_ui->current_page = page - 1;
    ui_alarm_refresh_list(s_ui);
    alarm_keyboard_hide();
    if (s_ui->panel_jump) {
        lv_obj_set_hidden(s_ui->panel_jump, true);
    }
}

static void alarm_btn_clear_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED || !s_ui) {
        return;
    }
    alarm_keyboard_hide();
    ui_alarm_clear_all();
    s_ui->current_page = 0;
    ui_alarm_refresh_list(s_ui);
    ESP_LOGI(TAG, "alarm list cleared (demo)");
}

static void alarm_btn_back_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    alarm_keyboard_hide();
    if (s_ui && s_ui->panel_jump) {
        lv_obj_set_hidden(s_ui->panel_jump, true);
    }
    ui_change_screen(UI_SCR_ui_home);
}

void ui_alarm_events_on_screen_active(ui_alarm_t *ui)
{
    if (!ui) {
        return;
    }
    s_ui = ui;
    alarm_keyboard_hide();
    if (ui->panel_jump) {
        lv_obj_set_hidden(ui->panel_jump, true);
    }
    ui_alarm_refresh_list(ui);
}

void ui_alarm_events_register(ui_alarm_t *ui)
{
    if (!ui || !ui->screen) {
        return;
    }

    s_ui = ui;
    app_lvgl_bind_swipe(ui->screen, (int)UI_SCR_ui_alarm);

    if (ui->keyboard) {
        lv_obj_add_event_cb(ui->keyboard, alarm_keyboard_extra_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }

    if (ui->ta_jump_page) {
        lv_obj_add_event_cb(ui->ta_jump_page, alarm_jump_ta_event_cb, LV_EVENT_ALL, NULL);
    }

    if (ui->btn_prev) {
        lv_obj_add_event_cb(ui->btn_prev, alarm_btn_prev_cb, LV_EVENT_CLICKED, NULL);
    }
    if (ui->btn_next) {
        lv_obj_add_event_cb(ui->btn_next, alarm_btn_next_cb, LV_EVENT_CLICKED, NULL);
    }
    if (ui->btn_jump) {
        lv_obj_add_event_cb(ui->btn_jump, alarm_btn_jump_cb, LV_EVENT_CLICKED, NULL);
    }
    if (ui->btn_clear) {
        lv_obj_add_event_cb(ui->btn_clear, alarm_btn_clear_cb, LV_EVENT_CLICKED, NULL);
    }
    if (ui->btn_back) {
        lv_obj_add_event_cb(ui->btn_back, alarm_btn_back_cb, LV_EVENT_CLICKED, NULL);
    }
    if (ui->btn_jump_ok) {
        lv_obj_add_event_cb(ui->btn_jump_ok, alarm_btn_jump_ok_cb, LV_EVENT_CLICKED, NULL);
    }
}
