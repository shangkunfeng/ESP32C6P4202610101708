#include "common.h"

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/task.h"

#include "sdkconfig.h"

uint16_t g_ui_fps;
uint8_t g_ui_cpu_usage_pct;

static uint32_t s_frame_count;
static int64_t s_fps_window_start_us;

static configRUN_TIME_COUNTER_TYPE read_run_time_counter(void)
{
#ifdef portALT_GET_RUN_TIME_COUNTER_VALUE
    configRUN_TIME_COUNTER_TYPE t = 0;
    portALT_GET_RUN_TIME_COUNTER_VALUE(t);
    return t;
#else
    return (configRUN_TIME_COUNTER_TYPE)portGET_RUN_TIME_COUNTER_VALUE();
#endif
}

static uint8_t sample_cpu_usage_pct(void)
{
#if !(CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS && INCLUDE_xTaskGetIdleTaskHandle)
    return 0;
#else
    const configRUN_TIME_COUNTER_TYPE total = read_run_time_counter();

    static configRUN_TIME_COUNTER_TYPE s_last_total;
    static configRUN_TIME_COUNTER_TYPE s_last_idle[CONFIG_FREERTOS_NUMBER_OF_CORES];
    static bool s_have_last;

    if (!s_have_last) {
        s_last_total = total;
        for (BaseType_t core = 0; core < CONFIG_FREERTOS_NUMBER_OF_CORES; core++) {
            s_last_idle[core] = ulTaskGetIdleRunTimeCounterForCore(core);
        }
        s_have_last = true;
        return 0;
    }

    if (total <= s_last_total) {
        s_last_total = total;
        for (BaseType_t core = 0; core < CONFIG_FREERTOS_NUMBER_OF_CORES; core++) {
            s_last_idle[core] = ulTaskGetIdleRunTimeCounterForCore(core);
        }
        return g_ui_cpu_usage_pct;
    }

    const configRUN_TIME_COUNTER_TYPE dt = total - s_last_total;
    s_last_total = total;

    if (dt == 0) {
        return g_ui_cpu_usage_pct;
    }

    /* 双核不能把 IDLE 时间相加再与 dt 比，否则轻载时 di≈2*dt 会被钳成 0% 占用 */
    uint32_t busy_pct_sum = 0;
    for (BaseType_t core = 0; core < CONFIG_FREERTOS_NUMBER_OF_CORES; core++) {
        const configRUN_TIME_COUNTER_TYPE idle_c = ulTaskGetIdleRunTimeCounterForCore(core);
        configRUN_TIME_COUNTER_TYPE di = idle_c - s_last_idle[core];
        s_last_idle[core] = idle_c;
        if (di > dt) {
            di = dt;
        }
        busy_pct_sum += (uint32_t)((dt - di) * 100U / dt);
    }

    const uint32_t pct = busy_pct_sum / (uint32_t)CONFIG_FREERTOS_NUMBER_OF_CORES;
    return (uint8_t)(pct > 100U ? 100U : pct);
#endif
}

void common_perf_on_frame(void)
{
    s_frame_count++;
}

void common_perf_service(void)
{
    const int64_t now_us = esp_timer_get_time();

    if (s_fps_window_start_us == 0) {
        s_fps_window_start_us = now_us;
    }

    const int64_t elapsed_us = now_us - s_fps_window_start_us;
    if (elapsed_us >= 1000000) {
        g_ui_fps = (uint16_t)((s_frame_count * 1000000ULL) / (uint64_t)elapsed_us);
        s_frame_count = 0;
        s_fps_window_start_us = now_us;
        g_ui_cpu_usage_pct = sample_cpu_usage_pct();
    }
}