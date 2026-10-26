#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LCD_COLOR_WHITE  0xFFFF
#define LCD_COLOR_RED    0xF800
#define LCD_COLOR_GREEN  0x07E0
#define LCD_COLOR_BLUE   0x001F

/** MIPI DSI + JD9165 上电与 panel 初始化；内部会调用 driver_draw_init 建立双缓冲 */
int driver_lcd_init(void);

/** 供 driver_draw 使用的 esp_lcd_panel_handle_t（void * 避免头文件依赖 esp_lcd） */
void *driver_lcd_get_panel(void);

/**
 * @brief 提交已写满的 back 帧缓冲（双缓冲「翻页 + 垂直消隐」）
 *
 * 实现见 driver_draw.c。必须在 back 缓冲上完成整帧绘制后再调用。
 * 内部流程：draw_bitmap(全屏) → 等待 on_refresh_done(VSync) → 更新 front/back 角色。
 *
 * 与全屏填色、后续 LVGL flush 共用同一路径，保证无撕裂。
 *
 * @param back_fb_rgb565 驱动分配的双缓冲之一（esp_lcd_dpi_panel_get_frame_buffer 所得）
 * @return ESP_OK 或 esp_err_t 错误码
 */
int driver_lcd_present_framebuffer(uint16_t *back_fb_rgb565);

/**
 * @brief 全屏 RGB565 纯色（内部：取 back → 填色 → present → 等 VSync）
 *
 * 自测红/绿/蓝请只调用本接口，不要绕过 present 直接 draw_bitmap。
 */
int driver_lcd_fill_screen(uint16_t color_rgb565);

/** 双缓冲指针（须 driver_lcd_init 完成） */
esp_err_t driver_lcd_get_frame_buffers(uint16_t **fb0, uint16_t **fb1);

/** 当前 front 下标 0/1，供 LVGL port 选择初始 back */
int driver_lcd_get_front_index(void);

/** 当前可写入的 back 帧缓冲（非正在扫描的 front） */
esp_err_t driver_lcd_get_back_framebuffer(uint16_t **fb_out);

/**
 * @brief PARTIAL 刷新前：将当前 front 整帧拷贝到 back
 *
 * LVGL 只往 back 写脏区时，back 其余像素须与屏上画面一致，否则 present 整帧会闪屏。
 * 在每帧第一次 flush 脏区之前调用；最终仍由 flush 末尾 msync + present。
 */
esp_err_t driver_lcd_sync_back_from_front(void);

#ifdef __cplusplus
}
#endif
