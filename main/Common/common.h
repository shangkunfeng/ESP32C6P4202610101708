#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 当前 UI 帧率（约每秒在 common_perf_service 中更新一次） */
extern uint16_t g_ui_fps;

/** 整机 CPU 占用估计 0–100（100 − 双核 IDLE 占比） */
extern uint8_t g_ui_cpu_usage_pct;

/** 每完成一帧 present 时调用（lv_port_disp 最后一笔 flush） */
void common_perf_on_frame(void);

/** 周期性调用：滚动 FPS 窗口并采样 CPU（由 ui_common 内 lv_timer 驱动） */
void common_perf_service(void);

#ifdef __cplusplus
}
#endif

#endif /* __COMMON_H__ */
