#pragma once

#include "esp_err.h"
#include "esp_eth.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * JC1060P470C 板载 RMII PHY（U4）+ ESP32-P4 EMAC。
 * 引脚见 net_phy_config.h；仅安装驱动，不创建 esp_netif（由 app_eth 负责）。
 */
esp_err_t driver_eth_init(void);

esp_err_t driver_eth_start(void);

bool driver_eth_is_inited(void);

esp_eth_handle_t driver_eth_get_handle(void);

/** link up 后打印速率/双工（需传入 CONNECTED 事件里的 handle） */
void driver_eth_log_link(esp_eth_handle_t handle);

#ifdef __cplusplus
}
#endif
