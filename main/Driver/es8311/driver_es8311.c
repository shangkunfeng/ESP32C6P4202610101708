#include "driver_es8311.h"

#include "driver_touch.h"
#include "es8311_config.h"

#include "driver/i2c_master.h"
#include "driver/i2s_common.h"
#include "driver/i2s_std.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

static const char *TAG = "driver_es8311";

static bool s_inited;
static bool s_capturing;
static i2c_master_dev_handle_t s_es8311_dev;
static i2s_chan_handle_t s_rx_chan;

static esp_err_t es8311_write_reg(uint8_t reg, uint8_t data)
{
    uint8_t buf[2] = {reg, data};
    return i2c_master_transmit(s_es8311_dev, buf, sizeof(buf), 1000);
}

static esp_err_t es8311_read_reg(uint8_t reg, uint8_t *data)
{
    return i2c_master_transmit_receive(s_es8311_dev, &reg, 1, data, 1, 1000);
}

/** 16 kHz @ MCLK = 4096000（256×fs），与 esp_codec 系数表一致 */
static esp_err_t es8311_configure_clock_16k(void)
{
    esp_err_t err;

    err = es8311_write_reg(0x02, 0x08);
    if (err != ESP_OK) {
        return err;
    }
    err = es8311_write_reg(0x03, 0x10);
    if (err != ESP_OK) {
        return err;
    }
    err = es8311_write_reg(0x04, 0x20);
    if (err != ESP_OK) {
        return err;
    }
    err = es8311_write_reg(0x05, 0x00);
    if (err != ESP_OK) {
        return err;
    }
    err = es8311_write_reg(0x06, 0x03);
    if (err != ESP_OK) {
        return err;
    }
    err = es8311_write_reg(0x07, 0x00);
    if (err != ESP_OK) {
        return err;
    }
    err = es8311_write_reg(0x08, 0xff);
    return err;
}

static esp_err_t es8311_codec_init_mic(void)
{
    esp_err_t err;

    err = es8311_write_reg(0x00, 0x1f);
    if (err != ESP_OK) {
        return err;
    }
    vTaskDelay(pdMS_TO_TICKS(20));
    err = es8311_write_reg(0x00, 0x00);
    if (err != ESP_OK) {
        return err;
    }
    err = es8311_write_reg(0x00, 0x80);
    if (err != ESP_OK) {
        return err;
    }
    err = es8311_write_reg(0x01, 0x3f);
    if (err != ESP_OK) {
        return err;
    }

    err = es8311_configure_clock_16k();
    if (err != ESP_OK) {
        return err;
    }

    uint8_t regv;
    err = es8311_read_reg(0x00, &regv);
    if (err != ESP_OK) {
        return err;
    }
    regv &= 0xbf;
    err = es8311_write_reg(0x00, regv);
    if (err != ESP_OK) {
        return err;
    }

    err = es8311_write_reg(0x09, 0x0c);
    if (err != ESP_OK) {
        return err;
    }
    err = es8311_write_reg(0x0a, 0x0c);
    if (err != ESP_OK) {
        return err;
    }
    err = es8311_write_reg(0x0d, 0x01);
    if (err != ESP_OK) {
        return err;
    }
    err = es8311_write_reg(0x0e, 0x02);
    if (err != ESP_OK) {
        return err;
    }
    err = es8311_write_reg(0x17, 0xc8);
    if (err != ESP_OK) {
        return err;
    }
    err = es8311_write_reg(0x14, 0x1a);
    if (err != ESP_OK) {
        return err;
    }
    err = es8311_write_reg(0x16, 0x01);

    return err;
}

static esp_err_t es7210_try_init(i2c_master_bus_handle_t bus)
{
    esp_err_t err = i2c_master_probe(bus, ES7210_I2C_ADDR, 200);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "ES7210 not on I2C (0x%02x), skip", ES7210_I2C_ADDR);
        return ESP_ERR_NOT_FOUND;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = ES7210_I2C_ADDR,
        .scl_speed_hz = ES8311_I2C_SCL_HZ,
    };
    i2c_master_dev_handle_t dev;
    err = i2c_master_bus_add_device(bus, &dev_cfg, &dev);
    if (err != ESP_OK) {
        return err;
    }

    const uint8_t seq[][2] = {
        {0x00, 0xff},
        {0x00, 0x41},
        {0x01, 0x3c},
        {0x06, 0x04},
        {0x08, 0x02},
        {0x09, 0x00},
    };
    for (size_t i = 0; i < sizeof(seq) / sizeof(seq[0]); i++) {
        err = i2c_master_transmit(dev, seq[i], 2, 200);
        if (err != ESP_OK) {
            break;
        }
    }
    i2c_master_bus_rm_device(dev);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "ES7210 I2C init OK");
    }
    return err;
}

static esp_err_t i2s_rx_init(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 6;
    chan_cfg.dma_frame_num = 240;

    esp_err_t err = i2s_new_channel(&chan_cfg, NULL, &s_rx_chan);
    if (err != ESP_OK) {
        return err;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(ES8311_DEFAULT_SAMPLE_RATE_HZ),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = (gpio_num_t)ES8311_I2S_MCLK_GPIO,
            .bclk = (gpio_num_t)ES8311_I2S_BCLK_GPIO,
            .ws = (gpio_num_t)ES8311_I2S_WS_GPIO,
            .dout = I2S_GPIO_UNUSED,
            .din = (gpio_num_t)ES8311_I2S_DIN_GPIO,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;

    return i2s_channel_init_std_mode(s_rx_chan, &std_cfg);
}

static esp_err_t require_inited(void)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

esp_err_t driver_es8311_init(void)
{
    if (s_inited) {
        return ESP_OK;
    }

    i2c_master_bus_handle_t bus = driver_touch_get_i2c_bus();
    if (!bus) {
        ESP_LOGE(TAG, "I2C bus missing (call driver_touch_init first)");
        return ESP_ERR_INVALID_STATE;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = ES8311_I2C_ADDR,
        .scl_speed_hz = ES8311_I2C_SCL_HZ,
    };
    esp_err_t err = i2c_master_bus_add_device(bus, &dev_cfg, &s_es8311_dev);
    ESP_RETURN_ON_ERROR(err, TAG, "es8311 i2c device");

    err = i2c_master_probe(bus, ES8311_I2C_ADDR, 200);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ES8311 probe 0x%02x failed", ES8311_I2C_ADDR);
        i2c_master_bus_rm_device(s_es8311_dev);
        s_es8311_dev = NULL;
        return err;
    }

    err = es8311_codec_init_mic();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ES8311 register init failed");
        i2c_master_bus_rm_device(s_es8311_dev);
        s_es8311_dev = NULL;
        return err;
    }

    (void)es7210_try_init(bus);

    err = i2s_rx_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2S RX init failed");
        i2c_master_bus_rm_device(s_es8311_dev);
        s_es8311_dev = NULL;
        return err;
    }

    s_capturing = false;
    s_inited = true;
    ESP_LOGI(TAG, "ready I2S RX din=%d mclk=%d bclk=%d ws=%d fs=%d",
             ES8311_I2S_DIN_GPIO, ES8311_I2S_MCLK_GPIO, ES8311_I2S_BCLK_GPIO, ES8311_I2S_WS_GPIO,
             ES8311_DEFAULT_SAMPLE_RATE_HZ);
    return ESP_OK;
}

esp_err_t driver_es8311_deinit(void)
{
    if (!s_inited) {
        return ESP_OK;
    }

    if (s_capturing) {
        (void)i2s_channel_disable(s_rx_chan);
        s_capturing = false;
    }
    if (s_rx_chan) {
        (void)i2s_del_channel(s_rx_chan);
        s_rx_chan = NULL;
    }
    if (s_es8311_dev) {
        (void)i2c_master_bus_rm_device(s_es8311_dev);
        s_es8311_dev = NULL;
    }

    s_inited = false;
    return ESP_OK;
}

bool driver_es8311_is_inited(void)
{
    return s_inited;
}

esp_err_t driver_es8311_start_capture(void)
{
    esp_err_t err = require_inited();
    if (err != ESP_OK) {
        return err;
    }
    if (s_capturing) {
        return ESP_OK;
    }

    err = i2s_channel_enable(s_rx_chan);
    ESP_RETURN_ON_ERROR(err, TAG, "i2s enable");
    s_capturing = true;
    return ESP_OK;
}

esp_err_t driver_es8311_stop_capture(void)
{
    esp_err_t err = require_inited();
    if (err != ESP_OK) {
        return err;
    }
    if (!s_capturing) {
        return ESP_OK;
    }

    err = i2s_channel_disable(s_rx_chan);
    ESP_RETURN_ON_ERROR(err, TAG, "i2s disable");
    s_capturing = false;
    return ESP_OK;
}

esp_err_t driver_es8311_read_pcm(int16_t *samples, size_t frame_count, uint8_t channels, size_t *frames_read)
{
    esp_err_t err = require_inited();
    if (err != ESP_OK) {
        return err;
    }
    if (!s_capturing) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!samples || !frames_read || channels == 0 || frame_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    const size_t bytes_want = frame_count * channels * sizeof(int16_t);
    size_t bytes_read = 0;
    err = i2s_channel_read(s_rx_chan, samples, bytes_want, &bytes_read, pdMS_TO_TICKS(500));
    if (err != ESP_OK) {
        return err;
    }

    const size_t samples_read = bytes_read / (channels * sizeof(int16_t));
    *frames_read = samples_read / channels;
    return ESP_OK;
}
