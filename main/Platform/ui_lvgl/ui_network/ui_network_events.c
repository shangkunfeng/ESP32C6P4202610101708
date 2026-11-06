#include "ui_network_events.h"

#include "app_lvgl.h"
#include "ui_common.h"

#include "esp_log.h"

static const char *TAG = "ui_network_evt";

static void network_btn_back_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ESP_LOGI(TAG, "back to ui_home");
    ui_change_screen(UI_SCR_ui_home);
}

void ui_network_events_register(ui_network_t *ui)
{
    if (!ui) {
        return;
    }

    app_lvgl_bind_swipe(ui->screen, (int)UI_SCR_ui_network);

    if (ui->btn_back) {
        lv_obj_add_event_cb(ui->btn_back, network_btn_back_cb, LV_EVENT_CLICKED, NULL);
    }
}
