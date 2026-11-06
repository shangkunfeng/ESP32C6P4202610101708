#pragma once

#include "ui_network.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_network_events_register(ui_network_t *ui);
void ui_network_events_on_screen_leave(ui_network_t *ui);

#ifdef __cplusplus
}
#endif
