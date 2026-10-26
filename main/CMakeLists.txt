set(UI_LVGL_SRCS
    "Platform/ui_lvgl/ui_login/ui_login.c"
    "Platform/ui_lvgl/ui_login/ui_login_events.c"
    "Platform/ui_lvgl/ui_login/fonts/ui_font_login_16.c"
    "Platform/ui_lvgl/ui_login/fonts/ui_font_login_22.c"
)
set(UI_LVGL_INCLUDES "Platform/ui_lvgl/ui_login")

list(APPEND UI_LVGL_SRCS
    "Platform/ui_lvgl/ui_home/ui_home.c"
    "Platform/ui_lvgl/ui_home/ui_home_events.c"
    "Platform/ui_lvgl/ui_home/fonts/ui_font_home_16.c"
    "Platform/ui_lvgl/ui_home/fonts/ui_font_home_22.c"
)
list(APPEND UI_LVGL_INCLUDES "Platform/ui_lvgl/ui_home")

list(APPEND UI_LVGL_SRCS
    "Platform/ui_lvgl/ui_mode/ui_mode.c"
    "Platform/ui_lvgl/ui_mode/ui_mode_events.c"
)
list(APPEND UI_LVGL_INCLUDES "Platform/ui_lvgl/ui_mode")

list(APPEND UI_LVGL_SRCS
    "Platform/ui_lvgl/ui_wifi/ui_wifi.c"
    "Platform/ui_lvgl/ui_wifi/ui_wifi_events.c"
)
list(APPEND UI_LVGL_INCLUDES "Platform/ui_lvgl/ui_wifi")

list(APPEND UI_LVGL_SRCS
    "Platform/ui_lvgl/ui_trend/ui_trend.c"
    "Platform/ui_lvgl/ui_trend/ui_trend_events.c"
)
list(APPEND UI_LVGL_INCLUDES "Platform/ui_lvgl/ui_trend")

list(APPEND UI_LVGL_SRCS
    "Platform/ui_lvgl/ui_param/ui_param.c"
    "Platform/ui_lvgl/ui_param/ui_param_events.c"
)
list(APPEND UI_LVGL_INCLUDES "Platform/ui_lvgl/ui_param")

list(APPEND UI_LVGL_SRCS
    "Platform/ui_lvgl/ui_alarm/ui_alarm.c"
    "Platform/ui_lvgl/ui_alarm/ui_alarm_events.c"
)
list(APPEND UI_LVGL_INCLUDES "Platform/ui_lvgl/ui_alarm")

idf_component_register(
    SRCS
        "main.c"
        "Common/common.c"
        "Application/app_sd_background.c"
        "Application/app_param_config.c"
        "Application/app_run_mode.c"
        "Application/app_history_store.c"
        "Application/app_init.c"
        "Application/app_tasks.c"
        "Application/app_lvgl.c"
        "Application/app_wifi.c"
        "Board/board_display.c"
        "Driver/lcd/esp_lcd_jd9165.c"
        "Driver/lcd/driver_lcd.c"
        "Driver/lcd/driver_draw.c"
        "Driver/touch/driver_touch.c"
        "Driver/sd_tf/driver_sd_tf.c"
        "Driver/co2/driver_co2.c"
        "Driver/temp/driver_temp.c"
        "Driver/hum/driver_hum.c"
        "Driver/rpm/driver_rpm.c"
        "Driver/hum/driver_hum.c"
        "Driver/rpm/driver_rpm.c"
        "Middleware/lv_port/lv_port_disp.c"
        "Middleware/lv_port/lv_port_indev.c"
        ${UI_LVGL_SRCS}
        "Platform/ui_lvgl/ui_common/ui_common.c"
    INCLUDE_DIRS
        "."
        "Application"
        "Board"
        "Common"
        "Config"
        "Driver/lcd"
        "Driver/touch"
        "Driver/sd_tf"
        "Driver/co2"
        "Driver/temp"
        "Driver/hum"
        "Driver/rpm"
        "Middleware"
        "Middleware/lv_port"
        "Platform"
        "Platform/ui_lvgl/ui_common"
        ${UI_LVGL_INCLUDES}
    REQUIRES
        log
        driver
        esp_driver_ledc
        esp_lcd
        esp_hw_support
        heap
        esp_mm
        freertos
        lvgl
        esp_timer
        esp_driver_i2c
        esp_lcd_touch
        esp_lcd_touch_gt911
        fatfs
        esp_driver_sdmmc
        sdmmc
        cjson
        nvs_flash
        esp_wifi
        esp_netif
        esp_event
        esp_hosted
    )
