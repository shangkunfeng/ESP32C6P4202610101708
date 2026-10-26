/**
 * LVGL 9 display port — PARTIAL 绘制 + 硬件双缓冲 present
 *
 *   LV_DISPLAY_RENDER_MODE_PARTIAL：LVGL 在小块 draw_buf 中渲染脏区
 *   → flush_cb: 每帧首笔先把 front 拷到 back，再写脏区（避免只改局部却整帧 present 闪屏）
 *   → 最后一笔 flush：整帧 cache C2M + driver_lcd_present_framebuffer + VSync
 *
 * 顺序：driver_lcd_init() → lv_init() → lv_port_disp_init()
 */

#include "lv_port_disp.h"

#include "common.h"
#include "display_config.h"
#include "driver_lcd.h"

#include "esp_cache.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lvgl.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static const char *TAG = "lv_port_disp";

static lv_display_t *s_disp;

/** 与 driver_draw 一致；1024×600×2 为 128B 整数倍，满足 esp_cache_msync 对齐 */
static const size_t s_fb_bytes =
    (size_t)LCD_H_RES * (size_t)LCD_V_RES * sizeof(uint16_t);

/** PARTIAL 条带高度（行）；越大 flush 次数越少，占 PSRAM 越多 */
#define LV_PORT_PARTIAL_LINES  48

#define LV_PORT_PARTIAL_BUF_BYTES \
    ((uint32_t)LCD_H_RES * (uint32_t)LV_PORT_PARTIAL_LINES * sizeof(uint16_t))

static uint16_t *s_partial_buf1;
static uint16_t *s_partial_buf2;

/** 本帧是否已用 front 填满 back（PARTIAL 与硬件双缓冲配合） */
static bool s_back_seeded_from_front;
static bool s_logged_first_flush;

/** P4 cache line 128B；DPI fb 基址未必对齐，按行扩展范围再 C2M */
static esp_err_t cache_writeback_range(void *ptr, size_t len)
{
    const size_t line = 128;
    const uintptr_t raw = (uintptr_t)ptr;
    const uintptr_t start = raw & ~(line - 1);
    const uintptr_t end = (raw + len + line - 1) & ~(line - 1);
    return esp_cache_msync((void *)start, end - start,
                           ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_TYPE_DATA);
}

static void frame_msync(uint16_t *fb)
{
    esp_err_t err = cache_writeback_range(fb, s_fb_bytes);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "frame msync %p failed: %s", (void *)fb, esp_err_to_name(err));
    }
}

static void partial_blit_to_back(uint16_t *fb, const lv_area_t *area, const uint8_t *px_map)
{
    int32_t x1 = area->x1;
    int32_t y1 = area->y1;
    int32_t x2 = area->x2;
    int32_t y2 = area->y2;

    if (x1 < 0) {
        x1 = 0;
    }
    if (y1 < 0) {
        y1 = 0;
    }
    if (x2 >= LCD_H_RES) {
        x2 = LCD_H_RES - 1;
    }
    if (y2 >= LCD_V_RES) {
        y2 = LCD_V_RES - 1;
    }
    if (x2 < x1 || y2 < y1) {
        return;
    }

    const int32_t w = x2 - x1 + 1;
    const int32_t h = y2 - y1 + 1;
    const uint16_t *src = (const uint16_t *)px_map;

    for (int32_t row = 0; row < h; row++) {
        uint16_t *dst = fb + (y1 + row) * LCD_H_RES + x1;
        memcpy(dst, src, (size_t)w * sizeof(uint16_t));
        src += w;
    }
}

/**
 * LVGL 显示刷新回调（lv_display_set_flush_cb 注册）。
 *
 * 渲染模式为 PARTIAL：LVGL 在内部条带缓冲（约 48 行高）里画好一块矩形后调用本函数，
 * 一帧 UI 更新往往会连续多次 flush（多块脏区 / 多条带）。
 *
 * 与 driver_draw 的配合：
 *   - 真正上屏的是硬件双缓冲中的「back」整帧（1024×600 RGB565，PSRAM）。
 *   - 本函数只把 px_map 代表的脏区贴到 back；整帧提交在「最后一次 flush」里 present。
 *   - present 前须 frame_msync，保证 CPU 写入 PSRAM 的数据 DMA 能读到。
 *
 * 必须在本函数返回前调用 lv_display_flush_ready(disp)，否则 LVGL 会阻塞不再渲染。
 *
 * @param disp   LVGL 显示对象
 * @param area   本次输出的矩形脏区（屏幕坐标，含 x1/y1/x2/y2）
 * @param px_map 脏区像素，RGB565，行优先，与 area 宽高一致（来自 s_partial_buf1/2）
 */
static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    /* 调试：全生命周期仅打印一次，确认 LVGL 已开始向显示驱动输出像素 */
    if (!s_logged_first_flush) 
    {
        s_logged_first_flush = true;
        ESP_LOGI(TAG, "first flush area (%d,%d)-(%d,%d)", (int)area->x1, (int)area->y1,
                 (int)area->x2, (int)area->y2);
    }

    uint16_t *back = NULL;

    /* 取当前可写的整屏 back（非正在扫描的 front），后续脏区都贴到此缓冲 */
    if (driver_lcd_get_back_framebuffer(&back) != ESP_OK || !back) 
    {
        ESP_LOGE(TAG, "flush: no back framebuffer");
        /* 即使失败也必须 flush_ready，避免 LVGL 永久等待 */
        lv_display_flush_ready(disp);
        return;
    }

    /*
     * PARTIAL 只更新脏区，present 却提交整屏 back。
     * 本帧第一次 flush 前：把屏上正在显示的 front 整帧拷到 back 打底，
     * 未重画的区域与上一帧一致，避免花屏/闪块。s_back_seeded_from_front 每帧末清零。
     */
    if (!s_back_seeded_from_front) 
    {
        if (driver_lcd_sync_back_from_front() != ESP_OK)
        {
            ESP_LOGE(TAG, "sync back from front failed");
        }

        s_back_seeded_from_front = true;
    }

    /* 将 LVGL 本条带/矩形像素写入 back 的对应坐标（仅改 area，其余保持打底内容） */
    partial_blit_to_back(back, area, px_map);

    /*
     * 本帧所有脏区都已 flush 完毕：统计 FPS、cache 写回、翻页并等 VSync 上屏。
     * 非最后一次 flush 只做 blit，不 present，减少无效等待。
     */
    if (lv_display_flush_is_last(disp))
    {
        common_perf_on_frame();
        s_back_seeded_from_front = false;
        frame_msync(back);
        
        if (driver_lcd_present_framebuffer(back) != ESP_OK) 
        {
            ESP_LOGE(TAG, "present failed");
        }
    }

    /* 通知 LVGL：本次 flush 处理结束，可继续下一块区域或下一帧 */
    lv_display_flush_ready(disp);
}

static esp_err_t partial_bufs_alloc(void)
{
    /* LVGL 条带缓冲放内部 RAM，减轻 PSRAM 上绘制/搬运的 cache 问题 */
    s_partial_buf1 = heap_caps_malloc(LV_PORT_PARTIAL_BUF_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    s_partial_buf2 = heap_caps_malloc(LV_PORT_PARTIAL_BUF_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    if (!s_partial_buf1 || !s_partial_buf2) 
    {
        if (s_partial_buf1) 
        {
            heap_caps_free(s_partial_buf1);
            s_partial_buf1 = NULL;
        }

        if (s_partial_buf2) 
        {
            heap_caps_free(s_partial_buf2);
            s_partial_buf2 = NULL;
        }

        const uint32_t caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
        s_partial_buf1 = heap_caps_malloc(LV_PORT_PARTIAL_BUF_BYTES, caps);
        s_partial_buf2 = heap_caps_malloc(LV_PORT_PARTIAL_BUF_BYTES, caps);
    }

    if (!s_partial_buf1 || !s_partial_buf2) {
        ESP_LOGE(TAG, "partial draw buffers alloc failed (%u bytes each)",
                 (unsigned)LV_PORT_PARTIAL_BUF_BYTES);
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t lv_port_disp_init(void)
{
    ESP_RETURN_ON_FALSE(driver_lcd_get_panel(), ESP_ERR_INVALID_STATE, TAG,
                        "call driver_lcd_init first");

    ESP_RETURN_ON_ERROR(partial_bufs_alloc(), TAG, "partial_bufs_alloc");

    s_disp = lv_display_create(LCD_H_RES, LCD_V_RES);
    ESP_RETURN_ON_FALSE(s_disp, ESP_ERR_NO_MEM, TAG, "lv_display_create failed");

    lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(s_disp, flush_cb);
    lv_display_set_buffers(s_disp, s_partial_buf1, s_partial_buf2, LV_PORT_PARTIAL_BUF_BYTES,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    ESP_LOGI(TAG, "disp %dx%d PARTIAL strip=%d lines buf=%u bytes x2",
             LCD_H_RES, LCD_V_RES, LV_PORT_PARTIAL_LINES, (unsigned)LV_PORT_PARTIAL_BUF_BYTES);
    return ESP_OK;
}
