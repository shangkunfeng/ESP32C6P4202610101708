#include "driver_eth.h"

#include "net_phy_config.h"

#include "esp_check.h"
#include "esp_eth.h"
#include "esp_eth_mac.h"
#include "esp_eth_mac_esp.h"
#include "esp_eth_phy.h"
#include "esp_eth_phy_ip101.h"
#include "esp_log.h"

static const char *TAG = "driver_eth";

static esp_eth_handle_t s_eth_handle;
static bool s_inited;
static bool s_started;

esp_err_t driver_eth_init(void)
{
    if (s_inited) {
        return ESP_OK;
    }

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = NET_PHY_ADDR;
    phy_config.reset_gpio_num = NET_PHY_RST_GPIO;
    phy_config.hw_reset_assert_time_us = 10000;
    phy_config.post_hw_reset_delay_ms = 100;

    eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    esp32_emac_config.smi_gpio.mdc_num = NET_PHY_MDC_GPIO;
    esp32_emac_config.smi_gpio.mdio_num = NET_PHY_MDIO_GPIO;
    /* RMII 数据/时钟与 IDF ESP32-P4 默认一致（与厂商 ethernet Demo 相同，勿随意改） */

    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);
    ESP_RETURN_ON_FALSE(mac != NULL, ESP_FAIL, TAG, "esp_eth_mac_new_esp32 failed");

    esp_eth_phy_t *phy = esp_eth_phy_new_ip101(&phy_config);
    if (phy == NULL) {
        mac->del(mac);
        return ESP_FAIL;
    }

    esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
    esp_err_t err = esp_eth_driver_install(&eth_config, &s_eth_handle);
    if (err != ESP_OK) {
        phy->del(phy);
        mac->del(mac);
        ESP_LOGE(TAG, "esp_eth_driver_install: %s", esp_err_to_name(err));
        return err;
    }

    s_inited = true;
    ESP_LOGI(TAG, "EMAC+" NET_PHY_PART " (IP101) phy=%d rst=%d mdc=%d mdio=%d",
             (int)NET_PHY_ADDR, NET_PHY_RST_GPIO, NET_PHY_MDC_GPIO, NET_PHY_MDIO_GPIO);
    return ESP_OK;
}

esp_err_t driver_eth_start(void)
{
    ESP_RETURN_ON_FALSE(s_inited && s_eth_handle != NULL, ESP_ERR_INVALID_STATE, TAG, "not inited");
    if (s_started) {
        return ESP_OK;
    }

    esp_err_t err = esp_eth_start(s_eth_handle);
    ESP_RETURN_ON_ERROR(err, TAG, "esp_eth_start");
    s_started = true;
    ESP_LOGI(TAG, "Ethernet driver started");
    return ESP_OK;
}

bool driver_eth_is_inited(void)
{
    return s_inited;
}

esp_eth_handle_t driver_eth_get_handle(void)
{
    return s_eth_handle;
}

void driver_eth_log_link(esp_eth_handle_t handle)
{
    if (handle == NULL) {
        return;
    }
    eth_speed_t speed = ETH_SPEED_10M;
    eth_duplex_t duplex = ETH_DUPLEX_HALF;
    if (esp_eth_ioctl(handle, ETH_CMD_G_SPEED, &speed) != ESP_OK) {
        ESP_LOGW(TAG, "ETH_CMD_G_SPEED failed");
    }
    if (esp_eth_ioctl(handle, ETH_CMD_G_DUPLEX_MODE, &duplex) != ESP_OK) {
        ESP_LOGW(TAG, "ETH_CMD_G_DUPLEX_MODE failed");
    }
    const char *spd = (speed == ETH_SPEED_100M) ? "100M" : (speed == ETH_SPEED_10M) ? "10M" : "?";
    const char *dpx = (duplex == ETH_DUPLEX_FULL) ? "full" : "half";
    ESP_LOGI(TAG, "link: %s %s duplex", spd, dpx);
}
