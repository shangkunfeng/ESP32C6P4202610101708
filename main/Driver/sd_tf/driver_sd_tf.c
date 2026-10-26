#include "driver_sd_tf.h"

#include "sd_tf_config.h"

#include "driver/gpio.h"
#include "driver/sdmmc_host.h"
#include "esp_check.h"
#include "esp_ldo_regulator.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdmmc_cmd.h"
#include "soc/soc_caps.h"

#include <stdio.h>
#include <sys/stat.h>

static const char *TAG = "driver_sd_tf";

static sdmmc_card_t *s_card;
static esp_ldo_channel_handle_t s_tf_ldo;
static bool s_mounted;
static SemaphoreHandle_t s_bus_lock;

#if !TF_PWR_GPIO_CTRL_NC
static esp_err_t tf_pwr_gpio_enable(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << TF_PWR_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&io), TAG, "tf pwr gpio");
    ESP_RETURN_ON_ERROR(gpio_set_level(TF_PWR_GPIO, 1), TAG, "tf pwr on");
    return ESP_OK;
}
#endif

#if SOC_SDMMC_IO_POWER_EXTERNAL
static esp_err_t tf_ldo_enable(void)
{
    const esp_ldo_channel_config_t cfg = {
        .chan_id = TF_LDO_PHY_CHAN,
        .voltage_mv = TF_LDO_PHY_MV,
    };
    ESP_RETURN_ON_ERROR(esp_ldo_acquire_channel(&cfg, &s_tf_ldo), TAG, "LDO VO4");
    ESP_LOGI(TAG, "TF LDO ch%d %dmV", TF_LDO_PHY_CHAN, TF_LDO_PHY_MV);
    vTaskDelay(pdMS_TO_TICKS(TF_LDO_POWERUP_DELAY_MS));
    return ESP_OK;
}

static void tf_ldo_disable(void)
{
    if (s_tf_ldo) {
        esp_ldo_release_channel(s_tf_ldo);
        s_tf_ldo = NULL;
    }
}
#endif

esp_err_t driver_sd_tf_init(void)
{
    if (s_mounted) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!s_bus_lock) {
        s_bus_lock = xSemaphoreCreateMutex();
        if (!s_bus_lock) {
            return ESP_ERR_NO_MEM;
        }
    }

    ESP_LOGI(TAG, "SDMMC CLK=%d CMD=%d D0=%d..D3=%d, mount %s",
             TF_SDMMC_CLK_GPIO, TF_SDMMC_CMD_GPIO,
             TF_SDMMC_D0_GPIO, TF_SDMMC_D3_GPIO, TF_VFS_MOUNT_POINT);

#if !TF_PWR_GPIO_CTRL_NC
    ESP_RETURN_ON_ERROR(tf_pwr_gpio_enable(), TAG, "tf pwr");
#endif

#if SOC_SDMMC_IO_POWER_EXTERNAL
    ESP_RETURN_ON_ERROR(tf_ldo_enable(), TAG, "tf ldo");
#endif

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    /* P4: SDMMC_HOST_DEFAULT() uses slot 1; ESP-Hosted (C6 SDIO) also uses slot 1.
     * TF 卡走 slot 0 + GPIO 39–44，必须与 Hosted 分开。 */
    host.slot = SDMMC_HOST_SLOT_0;
    host.io_voltage = 3.3f;
    host.pwr_ctrl_handle = NULL;
    host.flags &= ~SDMMC_HOST_FLAG_DDR;

    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = TF_SDMMC_BUS_WIDTH;
#if TF_CARD_DETECT_GPIO != PIN_NC
    slot.gpio_cd = TF_CARD_DETECT_GPIO;
#endif

#if SOC_SDMMC_USE_GPIO_MATRIX
    slot.clk = TF_SDMMC_CLK_GPIO;
    slot.cmd = TF_SDMMC_CMD_GPIO;
    slot.d0 = TF_SDMMC_D0_GPIO;
#if TF_SDMMC_BUS_WIDTH >= 4
    slot.d1 = TF_SDMMC_D1_GPIO;
    slot.d2 = TF_SDMMC_D2_GPIO;
    slot.d3 = TF_SDMMC_D3_GPIO;
#endif
#endif

    slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    const esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
        .format_if_mount_failed = TF_FORMAT_IF_MOUNT_FAILED,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };

#if TF_FORMAT_IF_MOUNT_FAILED
    ESP_LOGW(TAG, "TF_FORMAT_IF_MOUNT_FAILED=1: 无 FAT 分区时会格式化整张卡");
#endif

    static const int freq_khz_steps[] = {
        SDMMC_FREQ_PROBING,
        10000,
        TF_SDMMC_FREQ_LIMIT_HZ / 1000,
    };

    esp_err_t ret = ESP_FAIL;
    for (size_t i = 0; i < sizeof(freq_khz_steps) / sizeof(freq_khz_steps[0]); i++) {
        host.max_freq_khz = freq_khz_steps[i];
        ESP_LOGI(TAG, "mount try %u: max_freq_khz=%d", (unsigned)(i + 1), host.max_freq_khz);
        ret = esp_vfs_fat_sdmmc_mount(TF_VFS_MOUNT_POINT, &host, &slot, &mount_cfg, &s_card);
        if (ret == ESP_OK) {
            break;
        }
        ESP_LOGW(TAG, "mount try %u failed: %s", (unsigned)(i + 1), esp_err_to_name(ret));
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "mount failed: %s", esp_err_to_name(ret));
        ESP_LOGE(TAG, "0x107/TIMEOUT：多为未插卡、接触不良、引脚不符或卡损坏；请确认 TF 已插入且为 FAT32");
        ESP_LOGE(TAG, "CLK=%d CMD=%d D0..D3=%d..%d（见 sd_tf_config.h）",
                 TF_SDMMC_CLK_GPIO, TF_SDMMC_CMD_GPIO, TF_SDMMC_D0_GPIO, TF_SDMMC_D3_GPIO);
        ESP_LOGE(TAG, "若已识别卡但 mount 报 13：格式化为 FAT32，并建 %s/%s/", TF_VFS_MOUNT_POINT, TF_PHOTO_DIR_NAME);
        ESP_LOGE(TAG, "首次调试可将 TF_FORMAT_IF_MOUNT_FAILED 设为 1（会清空卡）");
#if SOC_SDMMC_IO_POWER_EXTERNAL
        tf_ldo_disable();
#endif
        return ret;
    }

    s_mounted = true;
    sdmmc_card_print_info(stdout, s_card);

    char photo_dir[64];
    snprintf(photo_dir, sizeof(photo_dir), "%s/%s", TF_VFS_MOUNT_POINT, TF_PHOTO_DIR_NAME);
    if (mkdir(photo_dir, 0775) != 0) {
        ESP_LOGI(TAG, "photo dir exists or mkdir skipped: %s", photo_dir);
    } else {
        ESP_LOGI(TAG, "created %s — copy ui_login/ui_home*.bin here", photo_dir);
    }

    char images_dir[64];
    snprintf(images_dir, sizeof(images_dir), "%s/%s", TF_VFS_MOUNT_POINT, TF_IMAGES_DIR_NAME);
    if (mkdir(images_dir, 0775) != 0) {
        ESP_LOGI(TAG, "images dir exists or mkdir skipped: %s", images_dir);
    } else {
        ESP_LOGI(TAG, "created %s — copy wifi_*.bin here", images_dir);
    }

    ESP_LOGI(TAG, "TF card ready at %s (须 FAT32，Windows 勿选 exFAT)", TF_VFS_MOUNT_POINT);
    return ESP_OK;
}

esp_err_t driver_sd_tf_deinit(void)
{
    if (!s_mounted) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_RETURN_ON_ERROR(esp_vfs_fat_sdcard_unmount(TF_VFS_MOUNT_POINT, s_card), TAG, "unmount");
    s_card = NULL;
    s_mounted = false;

#if SOC_SDMMC_IO_POWER_EXTERNAL
    tf_ldo_disable();
#endif

    return ESP_OK;
}

sdmmc_card_t *driver_sd_tf_get_card(void)
{
    return s_mounted ? s_card : NULL;
}

bool driver_sd_tf_bus_lock(TickType_t timeout_ticks)
{
    if (!s_bus_lock) {
        return false;
    }
    return xSemaphoreTake(s_bus_lock, timeout_ticks) == pdTRUE;
}

void driver_sd_tf_bus_unlock(void)
{
    if (s_bus_lock) {
        xSemaphoreGive(s_bus_lock);
    }
}
