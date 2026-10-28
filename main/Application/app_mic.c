#include "app_mic.h"

#include "driver_es8311.h"
#include "driver_uart1.h"
#include "es8311_config.h"
#include "uart1_config.h"

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdlib.h>

static const char *TAG = "app_mic";

static TaskHandle_t s_mic_task;
static bool s_mic_started;

#if UART1_MIC_STREAM_ENABLE
static void mic_uart1_send_pcm(const int16_t *pcm, size_t sample_count)
{
    if (!driver_uart1_is_inited() || pcm == NULL || sample_count == 0) {
        return;
    }

    uint8_t chunk[UART1_MIC_TX_CHUNK_BYTES];
    size_t chunk_len = 0;

    for (size_t i = 0; i < sample_count; i += (size_t)UART1_MIC_PCM_DECIM) {
        const int16_t s = pcm[i];
        chunk[chunk_len++] = (uint8_t)(s & 0xff);
        chunk[chunk_len++] = (uint8_t)((uint16_t)s >> 8);

        if (chunk_len + 2 > sizeof(chunk)) {
            size_t sent = 0;
            if (driver_uart1_write(chunk, chunk_len, &sent) != ESP_OK || sent != chunk_len) {
                static uint32_t warn_count;
                if ((warn_count++ % 200) == 0) {
                    ESP_LOGW(TAG, "uart1 pcm tx backlog");
                }
            }
            chunk_len = 0;
        }
    }

    if (chunk_len > 0) {
        size_t sent = 0;
        (void)driver_uart1_write(chunk, chunk_len, &sent);
    }
}
#endif

static void mic_capture_task(void *arg)
{
    (void)arg;

    const size_t frames = ES8311_PCM_FRAMES_PER_READ;
    const size_t samples_total = frames * ES8311_DEFAULT_CHANNELS;
    int16_t *pcm = (int16_t *)malloc(samples_total * sizeof(int16_t));
    if (!pcm) {
        ESP_LOGE(TAG, "pcm buffer alloc failed");
        vTaskDelete(NULL);
        return;
    }

    esp_err_t err = driver_es8311_start_capture();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "start_capture: %s", esp_err_to_name(err));
        free(pcm);
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "capture running (%u frames/read)", (unsigned)frames);
#if UART1_MIC_STREAM_ENABLE
    if (driver_uart1_is_inited()) {
        const unsigned eff_hz =
            ES8311_DEFAULT_SAMPLE_RATE_HZ / (unsigned)UART1_MIC_PCM_DECIM;
        ESP_LOGI(TAG, "uart1 pcm stream: int16 LE mono ~%u Hz (decim=%d)",
                 eff_hz, UART1_MIC_PCM_DECIM);
    }
#endif
    uint32_t loop = 0;

    for (;;) {
        size_t frames_read = 0;
        err = driver_es8311_read_pcm(pcm, frames, ES8311_DEFAULT_CHANNELS, &frames_read);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "read_pcm: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

#if UART1_MIC_STREAM_ENABLE
        if (frames_read > 0) {
            mic_uart1_send_pcm(pcm, frames_read * ES8311_DEFAULT_CHANNELS);
        }
#endif

        if ((loop++ % 50) == 0 && frames_read > 0) {
            int32_t peak = 0;
            for (size_t i = 0; i < frames_read * ES8311_DEFAULT_CHANNELS; i++) {
                int32_t v = pcm[i];
                if (v < 0) {
                    v = -v;
                }
                if (v > peak) {
                    peak = v;
                }
            }
            ESP_LOGI(TAG, "pcm peak=%ld (frames=%u)", (long)peak, (unsigned)frames_read);
        }
    }
}

esp_err_t app_mic_start(void)
{
    if (s_mic_started) {
        return ESP_OK;
    }
    ESP_RETURN_ON_FALSE(driver_es8311_is_inited(), ESP_ERR_INVALID_STATE, TAG,
                        "driver_es8311_init first");

    BaseType_t ok = xTaskCreate(mic_capture_task,
                                ES8311_MIC_TASK_NAME,
                                ES8311_MIC_TASK_STACK_WORDS,
                                NULL,
                                ES8311_MIC_TASK_PRIO,
                                &s_mic_task);
    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_ERR_NO_MEM, TAG, "xTaskCreate mic_cap");

    s_mic_started = true;
    ESP_LOGI(TAG, "task \"%s\" prio=%d stack=%u", ES8311_MIC_TASK_NAME, ES8311_MIC_TASK_PRIO,
             (unsigned)ES8311_MIC_TASK_STACK_WORDS);
    return ESP_OK;
}
