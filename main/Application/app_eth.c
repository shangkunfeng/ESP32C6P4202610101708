#include "app_eth.h"

#include "app_network_config.h"
#include "driver_eth.h"

#include "esp_check.h"
#include "esp_eth_netif_glue.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "app_eth";

static bool s_net_stack_ready;

static esp_err_t ensure_network_stack(void)
{
    if (s_net_stack_ready) {
        return ESP_OK;
    }

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(err, TAG, "nvs_flash_init");

    err = esp_netif_init();
    ESP_RETURN_ON_ERROR(err, TAG, "esp_netif_init");

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    s_net_stack_ready = true;
    return ESP_OK;
}

static bool s_ready;
static bool s_link_up;
static char s_ip[16];

static esp_netif_t *s_eth_netif;
static esp_eth_netif_glue_handle_t s_eth_glue;

static app_eth_ui_notify_fn s_ui_notify;
static void *s_ui_notify_ud;

static esp_timer_handle_t s_dhcp_watchdog;

static void notify_ui(void);

static void restart_dhcp_client(void)
{
#if APP_ETH_USE_STATIC_IP
    return;
#endif
    if (s_eth_netif == NULL) {
        return;
    }
    esp_err_t err = esp_netif_dhcpc_stop(s_eth_netif);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
        ESP_LOGW(TAG, "dhcpc_stop: %s", esp_err_to_name(err));
    }
    err = esp_netif_dhcpc_start(s_eth_netif);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
        ESP_LOGW(TAG, "dhcpc_restart: %s", esp_err_to_name(err));
    } else {
        ESP_LOGW(TAG, "DHCP client restarted (no offer yet?)");
    }
}

#if APP_ETH_USE_STATIC_IP
static void publish_static_ip_on_link(void)
{
    if (s_eth_netif == NULL) {
        return;
    }
    esp_netif_ip_info_t ip_info = {0};
    if (esp_netif_get_ip_info(s_eth_netif, &ip_info) != ESP_OK) {
        return;
    }
    if (ip_info.ip.addr == 0) {
        return;
    }
    snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&ip_info.ip));
    ESP_LOGI(TAG, "got IP %s (static) gw " IPSTR, s_ip, IP2STR(&ip_info.gw));
    if (s_dhcp_watchdog != NULL) {
        esp_timer_stop(s_dhcp_watchdog);
    }
    notify_ui();
}
#endif

static void log_dhcp_status(const char *when)
{
    if (s_eth_netif == NULL) {
        return;
    }
    esp_netif_dhcp_status_t st = ESP_NETIF_DHCP_INIT;
    esp_netif_ip_info_t info = {0};
    esp_netif_dhcpc_get_status(s_eth_netif, &st);
    esp_netif_get_ip_info(s_eth_netif, &info);
    ESP_LOGI(TAG, "%s: netif_up=%d dhcp=%d (0=INIT,1=STARTED,2=STOPPED) ip=" IPSTR,
             when, (int)esp_netif_is_netif_up(s_eth_netif), (int)st, IP2STR(&info.ip));
}

static void ensure_dhcp_client_running(void)
{
#if APP_ETH_USE_STATIC_IP
    return;
#endif
    if (s_eth_netif == NULL) {
        return;
    }
    esp_netif_dhcp_status_t st = ESP_NETIF_DHCP_INIT;
    if (esp_netif_dhcpc_get_status(s_eth_netif, &st) != ESP_OK) {
        return;
    }
    if (st == ESP_NETIF_DHCP_INIT || st == ESP_NETIF_DHCP_STOPPED) {
        esp_err_t err = esp_netif_dhcpc_start(s_eth_netif);
        if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
            ESP_LOGW(TAG, "dhcpc_start: %s", esp_err_to_name(err));
        }
    }
}

static void dhcp_watchdog_cb(void *arg)
{
    (void)arg;
    if (s_ip[0] != '\0') {
        return;
    }
    log_dhcp_status("still no IP");
    restart_dhcp_client();
    log_dhcp_status("after dhcp restart");
    ESP_LOGW(TAG, "若仍无 IP：确认接路由器 LAN 且开 DHCP，或设 APP_ETH_USE_STATIC_IP=1");
}

static void notify_ui(void)
{
    if (s_ui_notify) {
        s_ui_notify(s_ui_notify_ud);
    }
}

static void on_eth_event(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_base;

    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
        s_link_up = true;
        ESP_LOGI(TAG, "link up");
        if (event_data != NULL) {
            esp_eth_handle_t h = *(esp_eth_handle_t *)event_data;
            driver_eth_log_link(h);
        }
        esp_netif_set_default_netif(s_eth_netif);
        log_dhcp_status("after link up");
        ensure_dhcp_client_running();
#if APP_ETH_USE_STATIC_IP
        publish_static_ip_on_link();
#endif
        if (s_dhcp_watchdog != NULL) {
            esp_timer_stop(s_dhcp_watchdog);
            esp_timer_start_once(s_dhcp_watchdog, 15 * 1000000);
        }
        notify_ui();
        break;
    case ETHERNET_EVENT_DISCONNECTED:
        s_link_up = false;
        s_ip[0] = '\0';
        ESP_LOGI(TAG, "link down");
        notify_ui();
        break;
    case ETHERNET_EVENT_START:
        ESP_LOGI(TAG, "driver started");
        break;
    case ETHERNET_EVENT_STOP:
        ESP_LOGI(TAG, "driver stopped");
        break;
    default:
        break;
    }
}

static void on_eth_ip_event(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_base;

    if (event_id == IP_EVENT_ETH_GOT_IP && event_data != NULL) {
        const ip_event_got_ip_t *event = event_data;
        snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "got IP %s gw " IPSTR, s_ip, IP2STR(&event->ip_info.gw));
        if (s_dhcp_watchdog != NULL) {
            esp_timer_stop(s_dhcp_watchdog);
        }
        notify_ui();
        return;
    }
    if (event_id == IP_EVENT_ETH_LOST_IP) {
        s_ip[0] = '\0';
        ESP_LOGW(TAG, "lost IP");
        notify_ui();
    }
}

esp_err_t app_eth_init(void)
{
    if (s_ready) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "init begin");

    esp_err_t err = ensure_network_stack();
    ESP_RETURN_ON_ERROR(err, TAG, "ensure_network_stack");

    err = driver_eth_init();
    ESP_RETURN_ON_ERROR(err, TAG, "driver_eth_init");

    esp_eth_handle_t eth_handle = driver_eth_get_handle();
    ESP_RETURN_ON_FALSE(eth_handle != NULL, ESP_ERR_INVALID_STATE, TAG, "no eth handle");

    if (s_eth_netif == NULL) {
        esp_netif_config_t cfg = ESP_NETIF_DEFAULT_ETH();
        s_eth_netif = esp_netif_new(&cfg);
        ESP_RETURN_ON_FALSE(s_eth_netif != NULL, ESP_ERR_NO_MEM, TAG, "esp_netif_new eth");
    }

    if (s_eth_glue == NULL) {
        s_eth_glue = esp_eth_new_netif_glue(eth_handle);
        ESP_RETURN_ON_FALSE(s_eth_glue != NULL, ESP_ERR_NO_MEM, TAG, "esp_eth_new_netif_glue");
        err = esp_netif_attach(s_eth_netif, s_eth_glue);
        ESP_RETURN_ON_ERROR(err, TAG, "esp_netif_attach");
    }

#if APP_ETH_USE_STATIC_IP
    {
        err = esp_netif_dhcpc_stop(s_eth_netif);
        if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
            ESP_RETURN_ON_ERROR(err, TAG, "dhcpc_stop for static IP");
        }
        esp_netif_ip_info_t ip_info = {0};
        ESP_RETURN_ON_ERROR(esp_netif_str_to_ip4(APP_ETH_STATIC_IP, &ip_info.ip), TAG, "static ip");
        ESP_RETURN_ON_ERROR(esp_netif_str_to_ip4(APP_ETH_STATIC_GW, &ip_info.gw), TAG, "static gw");
        ESP_RETURN_ON_ERROR(esp_netif_str_to_ip4(APP_ETH_STATIC_NETMASK, &ip_info.netmask), TAG, "static mask");
        ESP_RETURN_ON_ERROR(esp_netif_set_ip_info(s_eth_netif, &ip_info), TAG, "set static ip");
        ESP_LOGI(TAG, "static IP " APP_ETH_STATIC_IP " gw " APP_ETH_STATIC_GW);
    }
#else
    ESP_LOGI(TAG, "using DHCP client");
#endif

    static bool handlers_registered;
    if (!handlers_registered) {
        err = esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &on_eth_event, NULL);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            return err;
        }
        err = esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &on_eth_ip_event, NULL);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            return err;
        }
        err = esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_LOST_IP, &on_eth_ip_event, NULL);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            return err;
        }
        const esp_timer_create_args_t wdg = {
            .callback = &dhcp_watchdog_cb,
            .name = "eth_dhcp_wdg",
        };
        if (esp_timer_create(&wdg, &s_dhcp_watchdog) != ESP_OK) {
            ESP_LOGW(TAG, "dhcp watchdog timer create failed");
        }
        handlers_registered = true;
    }

    err = driver_eth_start();
    ESP_RETURN_ON_ERROR(err, TAG, "driver_eth_start");

    esp_netif_set_default_netif(s_eth_netif);
    log_dhcp_status("after driver start");

    s_ready = true;
    ESP_LOGI(TAG, "Ethernet netif ready");
    return ESP_OK;
}

bool app_eth_is_ready(void)
{
    return s_ready;
}

bool app_eth_is_link_up(void)
{
    return s_link_up;
}

esp_err_t app_eth_get_ip_str(char *buf, size_t len)
{
    if (!buf || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_ip[0] == '\0') {
        snprintf(buf, len, "0.0.0.0");
        return ESP_OK;
    }
    snprintf(buf, len, "%s", s_ip);
    return ESP_OK;
}

void app_eth_set_ui_notify(app_eth_ui_notify_fn fn, void *user_data)
{
    s_ui_notify = fn;
    s_ui_notify_ud = user_data;
}
