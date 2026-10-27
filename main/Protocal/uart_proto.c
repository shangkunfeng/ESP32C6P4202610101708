#include "uart_proto.h"

#include "driver_uart.h"
#include "protocol_config.h"
#include "uart_proto_frame.h"
#include "uart_proto_info_table.h"
#include "uart_proto_payload.h"

#include "driver_co2.h"
#include "driver_hum.h"
#include "driver_rpm.h"
#include "driver_temp.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

static const char *TAG = "uart_proto";

#define UART_PROTO_RX_TASK_STACK    4096
#define UART_PROTO_RX_TASK_PRIO     (tskIDLE_PRIORITY + 4)
#define UART_PROTO_RX_CHUNK         128

static bool s_inited;
static bool s_rx_run;
static TaskHandle_t s_rx_task;
static uart_proto_ui_notify_fn s_ui_notify;

static esp_err_t uart_proto_ensure_uart(void)
{
<<<<<<< HEAD
    if (!driver_uart_is_inited()) 
    {
        esp_err_t err = driver_uart_init();

        if (err != ESP_OK) 
        {
            return err;
        }
    }

=======
    if (!driver_uart_is_inited()) {
        esp_err_t err = driver_uart_init();
        if (err != ESP_OK) {
            return err;
        }
    }
>>>>>>> 5c0939b1a3ae63d96b8bb499f5e2b674e3bfe0bf
    return ESP_OK;
}

void uart_proto_set_ui_notify(uart_proto_ui_notify_fn fn)
{
    s_ui_notify = fn;
}

static void uart_proto_notify_ui(void)
{
    if (s_ui_notify) {
        s_ui_notify();
    }
}

static esp_err_t uart_proto_dispatch_frame(const uart_proto_frame_t *frame)
{
<<<<<<< HEAD
    if (!frame) 
    {
=======
    if (!frame) {
>>>>>>> 5c0939b1a3ae63d96b8bb499f5e2b674e3bfe0bf
        return ESP_ERR_INVALID_ARG;
    }

    const uart_proto_info_entry_t *ent =
        uart_proto_info_table_find(frame->info_id, frame->info_cat);
    if (ent == NULL) {
        ESP_LOGW(TAG, "unknown frame id=0x%02X cat=0x%02X", frame->info_id, frame->info_cat);
        return ESP_ERR_NOT_FOUND;
    }
    if (frame->payload_len != ent->expected_length) {
        ESP_LOGW(TAG, "length mismatch id=0x%02X len=%u expect=%u",
                 frame->info_id, (unsigned)frame->payload_len, (unsigned)ent->expected_length);
        return ESP_ERR_INVALID_SIZE;
    }

    esp_err_t err = ESP_OK;
    if (frame->info_cat == PROTO_INFO_CAT_PERIOD_EVENT &&
        ent->flow == UART_PROTO_FLOW_DEVICE_TO_HOST) {
        err = uart_proto_payload_apply_real(frame->info_id, frame->payload, frame->payload_len);
        if (err == ESP_OK) {
            uart_proto_notify_ui();
        }
    } else if (frame->info_cat == PROTO_INFO_CAT_EVENT &&
               ent->flow == UART_PROTO_FLOW_HOST_TO_DEVICE) {
        err = uart_proto_payload_apply_set(frame->info_id, frame->payload, frame->payload_len);
        if (err == ESP_OK) {
            uart_proto_notify_ui();
        }
    } else {
        ESP_LOGW(TAG, "ignore frame id=0x%02X cat=0x%02X flow=%d",
                 frame->info_id, frame->info_cat, (int)ent->flow);
        return ESP_ERR_NOT_SUPPORTED;
    }

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "dispatch id=0x%02X cat=0x%02X: %s",
                 frame->info_id, frame->info_cat, esp_err_to_name(err));
    }
    return err;
}

static void uart_proto_rx_task(void *arg)
{
    (void)arg;
    uint8_t chunk[UART_PROTO_RX_CHUNK];

<<<<<<< HEAD
    while (s_rx_run) 
    {
        size_t n = 0;
        esp_err_t err = driver_uart_read(chunk, sizeof(chunk), &n, pdMS_TO_TICKS(50));

        if (err != ESP_OK || n == 0) 
        {
            continue;
        }

        for (size_t i = 0; i < n; i++) 
        {
            uart_proto_frame_t frame;
            bool ready = false;
            uart_proto_frame_feed(&chunk[i], 1, &frame, &ready);

            if (ready) 
            {
=======
    while (s_rx_run) {
        size_t n = 0;
        esp_err_t err = driver_uart_read(chunk, sizeof(chunk), &n, pdMS_TO_TICKS(50));
        if (err != ESP_OK || n == 0) {
            continue;
        }

        for (size_t i = 0; i < n; i++) {
            uart_proto_frame_t frame;
            bool ready = false;
            uart_proto_frame_feed(&chunk[i], 1, &frame, &ready);
            if (ready) {
>>>>>>> 5c0939b1a3ae63d96b8bb499f5e2b674e3bfe0bf
                uart_proto_dispatch_frame(&frame);
            }
        }
    }

    s_rx_task = NULL;
    vTaskDelete(NULL);
}

esp_err_t uart_proto_init(void)
{
<<<<<<< HEAD
    if (s_inited) 
    {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = uart_proto_ensure_uart();

    if (err != ESP_OK) 
    {
        ESP_LOGW(TAG, "uart driver: %s", esp_err_to_name(err));
    }

=======
    if (s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = uart_proto_ensure_uart();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "uart driver: %s", esp_err_to_name(err));
    }
>>>>>>> 5c0939b1a3ae63d96b8bb499f5e2b674e3bfe0bf
    uart_proto_frame_parser_reset();
    s_inited = true;
    ESP_LOGI(TAG, "uart_proto init");
    return ESP_OK;
}

esp_err_t uart_proto_deinit(void)
{
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    uart_proto_stop();
    s_inited = false;
    return ESP_OK;
}

esp_err_t uart_proto_start(void)
{
<<<<<<< HEAD
    if (!s_inited)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_rx_task) 
    {
=======
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_rx_task) {
>>>>>>> 5c0939b1a3ae63d96b8bb499f5e2b674e3bfe0bf
        return ESP_OK;
    }

    s_rx_run = true;
    BaseType_t ok = xTaskCreate(uart_proto_rx_task, "uart_proto", UART_PROTO_RX_TASK_STACK, NULL,
                                UART_PROTO_RX_TASK_PRIO, &s_rx_task);
<<<<<<< HEAD
    if (ok != pdPASS) 
    {
=======
    if (ok != pdPASS) {
>>>>>>> 5c0939b1a3ae63d96b8bb499f5e2b674e3bfe0bf
        s_rx_run = false;
        s_rx_task = NULL;
        return ESP_ERR_NO_MEM;
    }
<<<<<<< HEAD

=======
>>>>>>> 5c0939b1a3ae63d96b8bb499f5e2b674e3bfe0bf
    ESP_LOGI(TAG, "uart_proto rx task started");
    return ESP_OK;
}

void uart_proto_stop(void)
{
    s_rx_run = false;
<<<<<<< HEAD

    if (s_rx_task) 
    {
=======
    if (s_rx_task) {
>>>>>>> 5c0939b1a3ae63d96b8bb499f5e2b674e3bfe0bf
        vTaskDelete(s_rx_task);
        s_rx_task = NULL;
    }
}

esp_err_t uart_proto_send_set(uint8_t info_id, bool en)
{
    esp_err_t err = uart_proto_ensure_uart();
<<<<<<< HEAD

    if (err != ESP_OK) 
    {
=======
    if (err != ESP_OK) {
>>>>>>> 5c0939b1a3ae63d96b8bb499f5e2b674e3bfe0bf
        return err;
    }

    uint8_t payload[UART_PROTO_FRAME_MAX_PAYLOAD];
    size_t plen = 0;
    err = uart_proto_payload_pack_set(info_id, en, payload, sizeof(payload), &plen);
<<<<<<< HEAD

    if (err != ESP_OK) 
    {
        return err;
    }

    uart_proto_frame_t frame = 
    {
=======
    if (err != ESP_OK) {
        return err;
    }

    uart_proto_frame_t frame = {
>>>>>>> 5c0939b1a3ae63d96b8bb499f5e2b674e3bfe0bf
        .info_id = info_id,
        .info_cat = PROTO_INFO_CAT_EVENT,
        .payload_len = (uint16_t)plen,
    };
    memcpy(frame.payload, payload, plen);

    uint8_t wire[64];
    size_t wlen = 0;
    err = uart_proto_frame_build(&frame, wire, sizeof(wire), &wlen);
<<<<<<< HEAD

    if (err != ESP_OK) 
    {
=======
    if (err != ESP_OK) {
>>>>>>> 5c0939b1a3ae63d96b8bb499f5e2b674e3bfe0bf
        return err;
    }

    size_t sent = 0;
    err = driver_uart_write(wire, wlen, &sent);
<<<<<<< HEAD

    if (err != ESP_OK) 
    {
        return err;
    }
    if (sent != wlen) 
    {
        return ESP_FAIL;
    }
    
=======
    if (err != ESP_OK) {
        return err;
    }
    if (sent != wlen) {
        return ESP_FAIL;
    }
>>>>>>> 5c0939b1a3ae63d96b8bb499f5e2b674e3bfe0bf
    ESP_LOGD(TAG, "send EVENT set id=0x%02X en=%d", info_id, (int)en);
    return ESP_OK;
}

esp_err_t uart_proto_send_all_enable(bool en)
{
    co2_struct.En = en;
    temp_struct.En = en;
    hum_struct.En = en;
    rpm_struct.En = en;

    static const uint8_t ids[] = {
        PROTO_INFO_ID_CO2,
        PROTO_INFO_ID_TEMP,
        PROTO_INFO_ID_HUM,
        PROTO_INFO_ID_RPM,
    };

    esp_err_t first_err = ESP_OK;
    for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); i++) {
        esp_err_t err = uart_proto_send_set(ids[i], en);
        if (err != ESP_OK && first_err == ESP_OK) {
            first_err = err;
            ESP_LOGW(TAG, "send_set id=0x%02X: %s", ids[i], esp_err_to_name(err));
        }
    }
    uart_proto_notify_ui();
    return first_err;
}
