#pragma once

#include "ui_wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_wifi_events_register(ui_wifi_t *ui);
void ui_wifi_events_ap_row_cb(lv_event_t *e);

#ifdef __cplusplus
}
#endif
