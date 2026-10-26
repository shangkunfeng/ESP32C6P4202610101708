/**
 * ============================================================================
 * driver_draw.c — 双缓冲（Double Buffer）+ 垂直消隐（VSync）全屏绘制
 * ============================================================================
 *
 * 【硬件背景】
 *   本板 JD9165 + MIPI DPI，1024×600，RGB565，约 1.2 MB/帧。
 *   esp_lcd 在 JD9165_1024_600_PANEL_60HZ_DPI_CONFIG 里配置 num_fbs = 2，
 *   两块帧缓冲通常位于 PSRAM，由 DPI/MIPI 桥 DMA 按行扫描输出到屏。
 *
 * 【为何要双缓冲】
 *   若只有一块缓冲，CPU 改像素的同时 DMA 正在把同一块内存送给屏，
 *   会出现「撕裂」（画面上半是旧图、下半是新图）或随机花屏。
 *   双缓冲约定：
 *     - front：当前正在被扫描显示的那块（只读，应用层不要写）
 *     - back ：离屏缓冲，CPU/LVGL 在本帧全部画完后再「提交」
 *
 * 【为何要等垂直消隐 VSync】
 *   esp_lcd_panel_draw_bitmap() 传入 driver 内某块 fb 时，往往只是：
 *     ① 做 cache 写回；② 把「下一帧要扫的 fb 索引」指过去。
 *   真正切换 DMA 读指针，通常发生在**当前帧扫完**（垂直消隐沿）。
 *   因此必须在 draw_bitmap 之后等待 on_refresh_done 回调（等价于等到 VSync），
 *   才能确认：刚提交的 back 已经变成 front，原来的 front 才可以作为下一帧 back 安全写入。
 *
 * 【本文件标准三步（红/绿/蓝自测与后续 LVGL flush 同一模型）】
 *     ① lcd_get_back_fb()      — 取「非显示中」的 back 指针
 *     ② 在 back 上写满像素     — 如 lcd_fill_fb_rgb565()
 *     ③ driver_lcd_present_framebuffer() — draw_bitmap + lcd_wait_vsync()
 *
 * 【时序示意】
 *
 *      CPU 写 back          draw_bitmap(back)           VSync 到来
 *      ───────────►         ───────────────►            ─────────────►
 *      [ back 可写 ]        [ 排队下一帧 ]              [ back→front，可再写旧 front ]
 *
 *      ┌─────────┐ 正在扫     ┌─────────┐
 *      │ front   │ ────────► │  屏幕   │
 *      └─────────┘           └─────────┘
 *      ┌─────────┐
 *      │ back    │ ◄── 应用只在这里改像素
 *      └─────────┘
 *
 * 【注意】
 *   - on_refresh_done 在 ISR 里 Give 信号量，任务里 Take，不可在回调里做耗时操作。
 *   - PSRAM 写完后需 esp_cache_msync(C2M)，否则 DMA 可能读到旧 cache 行。
 *   - 禁止对 front 直接填色；禁止不等 VSync 连续 draw_bitmap 两块不同内容。
 * ============================================================================
 */

#include "driver_lcd.h"

#include "display_config.h"

#include "esp_attr.h"
#include "esp_cache.h"
#include "esp_check.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <stdint.h>
#include <string.h>

static const char *TAG = "driver_draw";

/** DPI 驱动分配的帧缓冲个数，与 esp_lcd_jd9165.h 中 num_fbs 一致 */
#define LCD_FB_COUNT        2

/** 等待 VSync 的最长时间（ms）。60 Hz 约 16.7 ms/帧，留余量防止偶发丢中断 */
#define LCD_VSYNC_TIMEOUT_MS 50

/** 两块帧缓冲的 CPU 侧指针，由 esp_lcd_dpi_panel_get_frame_buffer 取得 */
static uint16_t *s_fbs[LCD_FB_COUNT];

/**
 * 当前 front 的下标（0 或 1），表示「哪一块正在被 DMA 扫描显示」。
 * back 恒为 s_fbs[1 - s_front_idx]。
 * 每次 present 成功并等到 VSync 后更新。
 */
static int s_front_idx;

/** 二进制信号量：ISR 在 VSync 时 Give，任务在 present 后 Take，实现「等到翻页完成」 */
static SemaphoreHandle_t s_vsync_sem;

/** driver_draw_init 是否已成功（双缓冲指针与回调已就绪） */
static bool s_draw_ready;

/** 单帧字节数，用于 cache msync 范围 */
static const size_t s_fb_bytes =
    (size_t)LCD_H_RES * (size_t)LCD_V_RES * sizeof(uint16_t);

static esp_err_t cache_writeback_range(void *ptr, size_t len)
{
    const size_t line = 128;
    const uintptr_t raw = (uintptr_t)ptr;
    const uintptr_t start = raw & ~(line - 1);
    const uintptr_t end = (raw + len + line - 1) & ~(line - 1);
    return esp_cache_msync((void *)start, end - start,
                           ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_TYPE_DATA);
}

/**
 * @brief DPI 面板「一帧刷新完成」回调（垂直消隐 / VSync）
 *
 * 由 MIPI DPI 桥在**当前帧扫描结束**时触发，运行在中断上下文（ISR）。
 * 这里只做一件事：释放 s_vsync_sem，唤醒正在 driver_lcd_present_framebuffer()
 * 里等待的任务，表示「你刚才 draw_bitmap 指定的那块 fb，已经完成显示切换」。
 *
 * @return true  若唤醒了更高优先级任务，ISR 退出时需 portYIELD_FROM_ISR
 * @return false 无需额外调度
 *
 * @note 切勿在此回调里打大量日志、malloc 或访问 I2C/SPI 等会阻塞的外设。
 */
IRAM_ATTR static bool lcd_on_refresh_done(esp_lcd_panel_handle_t panel,
                                          esp_lcd_dpi_panel_event_data_t *edata,
                                          void *user_ctx)
{
    (void)panel;
    (void)edata;
    (void)user_ctx;

    BaseType_t hp_task_woken = pdFALSE;
    if (s_vsync_sem) 
    {
        xSemaphoreGiveFromISR(s_vsync_sem, &hp_task_woken);
    }

    return hp_task_woken == pdTRUE;
}

/**
 * @brief 初始化双缓冲绘制环境（须在 esp_lcd_panel_init 之后调用）
 *
 * 完成工作：
 *   1. 创建 VSync 用二进制信号量，并清空启动前可能残留的令牌；
 *   2. 向 DPI panel 注册 on_refresh_done → lcd_on_refresh_done；
 *   3. 从驱动取得 fb0、fb1 指针（内存由 esp_lcd / JD9165 配置分配，一般在 PSRAM）；
 *   4. 假定初始 front 为 fb0（与驱动默认扫描块一致）。
 *
 * @return ESP_OK 成功；否则见 esp_check 日志
 *
 * @note 可重复调用：已初始化时直接返回 ESP_OK。
 * @note 由 driver_lcd_init() 在 panel_init 之后调用，应用层不要单独跳过 panel 初始化。
 */
int driver_draw_init(void)
{
    if (s_draw_ready) 
    {
        return ESP_OK;
    }

    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)driver_lcd_get_panel();
    ESP_RETURN_ON_FALSE(panel, ESP_ERR_INVALID_STATE, TAG, "panel not ready");

    s_vsync_sem = xSemaphoreCreateBinary();
    ESP_RETURN_ON_FALSE(s_vsync_sem, ESP_ERR_NO_MEM, TAG, "vsync sem");

    /* 丢弃注册回调前可能已产生、但无人 Take 的 VSync 令牌，避免第一次 present 误判「已同步」 */
    while (xSemaphoreTake(s_vsync_sem, 0) == pdTRUE) {
    }

    const esp_lcd_dpi_panel_event_callbacks_t cbs = {
        .on_refresh_done = lcd_on_refresh_done,
    };
    ESP_RETURN_ON_ERROR(
        esp_lcd_dpi_panel_register_event_callbacks(panel, &cbs, NULL),
        TAG, "register vsync");

    ESP_RETURN_ON_ERROR(
        esp_lcd_dpi_panel_get_frame_buffer(panel, LCD_FB_COUNT,
                                           (void **)&s_fbs[0], (void **)&s_fbs[1]),
        TAG, "get_frame_buffer");

    ESP_RETURN_ON_FALSE(s_fbs[0] && s_fbs[1], ESP_ERR_NO_MEM, TAG, "frame buffers null");

    s_front_idx = 0;
    s_draw_ready = true;

    ESP_LOGI(TAG, "double-buffer + vsync fb0=%p fb1=%p", (void *)s_fbs[0], (void *)s_fbs[1]);
    return ESP_OK;
}

/**
 * @brief 获取当前可安全写入的 back 帧缓冲指针
 *
 * back = 非 s_front_idx 的那一块。CPU 填色、memcpy、LVGL 绘制都应只发生在 back 上。
 *
 * @param[out] fb_out 输出 back 指针（RGB565，大小 LCD_H_RES × LCD_V_RES）
 */
static esp_err_t lcd_get_back_fb(uint16_t **fb_out)
{
    ESP_RETURN_ON_FALSE(s_draw_ready, ESP_ERR_INVALID_STATE, TAG, "draw not init");
    *fb_out = s_fbs[1 - s_front_idx];
    return ESP_OK;
}

/** 校验指针是否为驱动分配的两块 fb 之一（禁止 present 栈上或堆上临时缓冲） */
static bool lcd_is_driver_fb(const uint16_t *fb)
{
    return fb == s_fbs[0] || fb == s_fbs[1];
}

/**
 * @brief 阻塞等待下一次垂直消隐（on_refresh_done）
 *
 * present 流程在 draw_bitmap 之后调用本函数，确保 DMA 已完成「从旧 front 切到新 front」，
 * 之后才能安全地把刚显示过的那块内存当作下一帧的 back 继续写入。
 *
 * @param timeout_ms 最长等待时间；超时返回 ESP_ERR_TIMEOUT（可能闪屏，需查 DSI/中断）
 */
static esp_err_t lcd_wait_vsync(uint32_t timeout_ms)
{
    ESP_RETURN_ON_FALSE(s_vsync_sem, ESP_ERR_INVALID_STATE, TAG, "vsync sem");

    if (xSemaphoreTake(s_vsync_sem, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) 
    {
        ESP_LOGW(TAG, "vsync timeout %lums", (unsigned long)timeout_ms);
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

/**
 * @brief 在指定 fb 内填充满屏 RGB565 纯色，并做 CPU cache → 内存 同步
 *
 * 仅负责「写 back 内容」，不触发翻页。翻页必须由 driver_lcd_present_framebuffer 完成。
 *
 * @param fb             目标缓冲，必须是 back（通常来自 lcd_get_back_fb）
 * @param color_rgb565   16 位色，如红 0xF800、绿 0x07E0、蓝 0x001F
 *
 * @note esp_cache_msync(C2M)：ESP32-P4 写 PSRAM 经 cache，不同步则 DMA 可能读到旧数据。
 */
static esp_err_t lcd_fill_fb_rgb565(uint16_t *fb, uint16_t color_rgb565)
{
    const size_t pixel_count = (size_t)LCD_H_RES * (size_t)LCD_V_RES;
    const uint32_t pair = ((uint32_t)color_rgb565 << 16) | (uint32_t)color_rgb565;

    if (((uintptr_t)fb & 3U) == 0 && pixel_count >= 2) 
    {
        uint32_t *words = (uint32_t *)fb;
        const size_t word_count = pixel_count / 2;

        for (size_t i = 0; i < word_count; i++) 
        {
            words[i] = pair;
        }

        if (pixel_count & 1U) 
        {
            fb[pixel_count - 1] = color_rgb565;
        }

    } 
    else 
    {
        for (size_t i = 0; i < pixel_count; i++) 
        {
            fb[i] = color_rgb565;
        }
    }

    return cache_writeback_range(fb, s_fb_bytes);
}

/**
 * @brief 提交 back 帧：翻页 + 等待垂直消隐（双缓冲核心步骤 ②③）
 *
 * 调用顺序与含义：
 *   1. 清空信号量中积压的旧 VSync 令牌（避免等到「上一次」的消隐而非本次 flip）；
 *   2. esp_lcd_panel_draw_bitmap(全屏, back)：告知 DPI 驱动下一帧从 back 取数；
 *   3. lcd_wait_vsync：阻塞直到 lcd_on_refresh_done 被 ISR 唤醒；
 *   4. 更新 s_front_idx，使 back/front 角色对调，供下一帧 lcd_get_back_fb 使用。
 *
 * @param back_fb_rgb565 已画好的 back 缓冲，必须是 s_fbs[0] 或 s_fbs[1]
 * @return ESP_OK 表示本帧已完整显示且无撕裂；超时或参数错误见返回值
 *
 * @warning 不要对正在显示的 front 调用本函数；不要在未写完 back 时调用。
 */
int driver_lcd_present_framebuffer(uint16_t *back_fb_rgb565)
{
    ESP_RETURN_ON_FALSE(back_fb_rgb565, ESP_ERR_INVALID_ARG, TAG, "back fb null");
    ESP_RETURN_ON_FALSE(lcd_is_driver_fb(back_fb_rgb565), ESP_ERR_INVALID_ARG, TAG,
                        "must be driver frame buffer");

    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)driver_lcd_get_panel();
    ESP_RETURN_ON_FALSE(panel, ESP_ERR_INVALID_STATE, TAG, "panel");

    while (xSemaphoreTake(s_vsync_sem, 0) == pdTRUE) 
    {
    }

    ESP_RETURN_ON_ERROR(
        esp_lcd_panel_draw_bitmap(panel, 0, 0, LCD_H_RES, LCD_V_RES, back_fb_rgb565),
        TAG, "draw_bitmap");

    ESP_RETURN_ON_ERROR(lcd_wait_vsync(LCD_VSYNC_TIMEOUT_MS), TAG, "wait vsync");

    if (back_fb_rgb565 == s_fbs[0]) 
    {
        s_front_idx = 0;
    } 
    else 
    {
        s_front_idx = 1;
    }

    ESP_LOGD(TAG, "present back=%p front_idx=%d", (void *)back_fb_rgb565, s_front_idx);
    return ESP_OK;
}

/**
 * @brief 全屏填充一种 RGB565 颜色（封装完整的双缓冲 + VSync 流程）
 *
 * 等价于：
 *   lcd_get_back_fb → lcd_fill_fb_rgb565(back) → driver_lcd_present_framebuffer(back)
 *
 * 红/绿/蓝自测在 main 里循环调用本函数即可；每种颜色都会完整走「写 back → 翻页 → 等 VSync」，
 * 避免有时能亮、有时撕裂或闪一下的问题。
 *
 * @param color_rgb565 全屏像素值
 */
esp_err_t driver_lcd_get_frame_buffers(uint16_t **fb0, uint16_t **fb1)
{
    ESP_RETURN_ON_FALSE(s_draw_ready && fb0 && fb1, ESP_ERR_INVALID_STATE, TAG, "draw not init");
    ESP_RETURN_ON_FALSE(s_fbs[0] && s_fbs[1], ESP_ERR_NO_MEM, TAG, "frame buffers null");
    *fb0 = s_fbs[0];
    *fb1 = s_fbs[1];
    return ESP_OK;
}

int driver_lcd_get_front_index(void)
{
    return s_front_idx;
}

esp_err_t driver_lcd_get_back_framebuffer(uint16_t **fb_out)
{
    return lcd_get_back_fb(fb_out);
}

esp_err_t driver_lcd_sync_back_from_front(void)
{
    ESP_RETURN_ON_FALSE(s_draw_ready, ESP_ERR_INVALID_STATE, TAG, "draw not init");
    ESP_RETURN_ON_FALSE(s_fbs[0] && s_fbs[1], ESP_ERR_INVALID_STATE, TAG, "frame buffers null");

    const uint16_t *front = s_fbs[s_front_idx];
    uint16_t *back = s_fbs[1 - s_front_idx];
    memcpy(back, front, s_fb_bytes);
    return cache_writeback_range(back, s_fb_bytes);
}

int driver_lcd_fill_screen(uint16_t color_rgb565)
{
    uint16_t *back = NULL;
    ESP_RETURN_ON_ERROR(lcd_get_back_fb(&back), TAG, "get back fb");

    ESP_RETURN_ON_ERROR(lcd_fill_fb_rgb565(back, color_rgb565), TAG, "fill back");

    ESP_RETURN_ON_ERROR(driver_lcd_present_framebuffer(back), TAG, "present");
    ESP_LOGI(TAG, "fill 0x%04X (back→present→vsync) front_idx=%d", color_rgb565, s_front_idx);
    return ESP_OK;
}
