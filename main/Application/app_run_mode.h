#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_RUN_MODE_MANUAL = 0,
    APP_RUN_MODE_AUTO = 1,
} app_run_mode_t;

app_run_mode_t app_run_mode_get(void);
void app_run_mode_set(app_run_mode_t mode);

/** 界面显示用："手动" / "自动" */
const char *app_run_mode_text(void);

/** "模式：手动" / "模式：自动" */
const char *app_run_mode_line_text(void);

#ifdef __cplusplus
}
#endif
