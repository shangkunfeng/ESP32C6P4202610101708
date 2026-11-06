#ifndef __APP_NETWORK_CONFIG_H__
#define __APP_NETWORK_CONFIG_H__

/**
 * 应用层网络任务开关（保留源码，仅控制 app_tasks 是否启动）。
 * 当前：关闭 Wi-Fi 任务，启用有线以太网任务。
 */

/** 1：app_tasks 中调用 app_wifi_init()（C6 STA） */
#define APP_WIFI_TASK_ENABLE        0

/** 1：app_tasks 中调用 app_eth_init()（P4 EMAC + PHY） */
#define APP_ETH_TASK_ENABLE         1

/**
 * 网线接 PC 直连（无路由器）时必须 APP_ETH_USE_STATIC_IP=1，且 PC 与板子用同一专用网段。
 *
 * 推荐直连网段 192.168.50.0/24（勿与 PC 上网用的 192.168.2.x 混用）：
 *   板子：192.168.50.2
 *   PC 上「插板子那根线」对应的网卡（多为「以太网 2」）手动设为 192.168.50.1 / 255.255.255.0，网关留空
 *   PC 上 ping 192.168.50.2
 *
 * 若板子与 PC 都接同一路由器 LAN（192.168.2.x）：
 *   APP_ETH_USE_STATIC_IP=0 用 DHCP，或静态 192.168.2.123 / gw 192.168.2.7
 */
#define APP_ETH_USE_STATIC_IP       0
#define APP_ETH_STATIC_IP           "192.168.50.2"
#define APP_ETH_STATIC_GW           "192.168.50.1"
#define APP_ETH_STATIC_NETMASK      "255.255.255.0"

#endif
