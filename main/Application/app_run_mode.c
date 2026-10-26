#include "app_run_mode.h"

static app_run_mode_t s_mode = APP_RUN_MODE_AUTO;

app_run_mode_t app_run_mode_get(void)
{
    return s_mode;
}

void app_run_mode_set(app_run_mode_t mode)
{
    if (mode != APP_RUN_MODE_MANUAL && mode != APP_RUN_MODE_AUTO) {
        return;
    }
    s_mode = mode;
}

const char *app_run_mode_text(void)
{
    return (s_mode == APP_RUN_MODE_AUTO) ? "自动" : "手动";
}

const char *app_run_mode_line_text(void)
{
    return (s_mode == APP_RUN_MODE_AUTO) ? "模式：自动" : "模式：手动";
}
