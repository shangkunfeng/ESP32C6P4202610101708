#include "driver_uart.h"

#include "uart_config.h"

#include "driver/uart.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <string.h>

static const char *TAG = "driver_uart";

/** UART ISR 投递事件 → uart_evt 任务（仅处理 RX 与错误） */
#define DRIVER_UART_EVENT_QUEUE_LEN     20
#define DRIVER_UART_RX_TASK_STACK       3072
#define DRIVER_UART_RX_TASK_PRIO        (tskIDLE_PRIORITY + 3)

/** 应用 write 入队，uart_tx 任务统一 uart_write_bytes */
#define DRIVER_UART_TX_QUEUE_DEPTH      16
#define DRIVER_UART_TX_PKT_MAX          512
#define DRIVER_UART_TX_TASK_STACK       3072
#define DRIVER_UART_TX_TASK_PRIO        (tskIDLE_PRIORITY + 2)
#define DRIVER_UART_TX_ENQUEUE_TICKS    pdMS_TO_TICKS(200)

#define DRIVER_UART_RX_RING_SIZE        (UART0_RX_BUF_SIZE * 2)
#define DRIVER_UART_EVT_READ_CHUNK      256
#define DRIVER_UART_RX_NOTIFY_MAX       32

typedef struct {
    size_t len;
    uint8_t data[DRIVER_UART_TX_PKT_MAX];
} driver_uart_tx_pkt_t;

static bool s_inited;
static QueueHandle_t s_uart_queue;
static TaskHandle_t s_rx_task;
static QueueHandle_t s_tx_queue;
static TaskHandle_t s_tx_task;
static SemaphoreHandle_t s_rx_mux;
static SemaphoreHandle_t s_rx_data_sem;
static uint8_t s_rx_ring[DRIVER_UART_RX_RING_SIZE];
static size_t s_rx_head;
static size_t s_rx_tail;
static driver_uart_rx_cb_t s_rx_cb;
static void *s_rx_cb_ctx;

static size_t rx_ring_used(void)
{
<<<<<<< HEAD
    if (s_rx_head >= s_rx_tail) 
    {
        return s_rx_head - s_rx_tail;
    }

=======
    if (s_rx_head >= s_rx_tail) {
        return s_rx_head - s_rx_tail;
    }
>>>>>>> 5c0939b1a3ae63d96b8bb499f5e2b674e3bfe0bf
    return DRIVER_UART_RX_RING_SIZE - s_rx_tail + s_rx_head;
}

static size_t rx_ring_free(void)
{
    return DRIVER_UART_RX_RING_SIZE - 1 - rx_ring_used();
}

static void rx_ring_push(const uint8_t *data, size_t len)
{
<<<<<<< HEAD
    for (size_t i = 0; i < len; i++) 
    {
=======
    for (size_t i = 0; i < len; i++) {
>>>>>>> 5c0939b1a3ae63d96b8bb499f5e2b674e3bfe0bf
        if (rx_ring_free() == 0) {
            s_rx_tail = (s_rx_tail + 1) % DRIVER_UART_RX_RING_SIZE;
        }
        s_rx_ring[s_rx_head] = data[i];
        s_rx_head = (s_rx_head + 1) % DRIVER_UART_RX_RING_SIZE;
    }
}

static size_t rx_ring_pop(uint8_t *dst, size_t max_len)
{
    size_t n = 0;
    while (n < max_len && s_rx_tail != s_rx_head) {
        dst[n++] = s_rx_ring[s_rx_tail];
        s_rx_tail = (s_rx_tail + 1) % DRIVER_UART_RX_RING_SIZE;
    }
    return n;
}

static void rx_notify_data_ready(void)
{
    if (s_rx_data_sem) {
        (void)xSemaphoreGive(s_rx_data_sem);
    }
}

/** 硬件 RX 中断 → IDF 驱动缓冲 → UART_DATA 事件，在此入环并唤醒读侧 */
static void uart_on_rx_bytes(const uint8_t *data, size_t len)
{
    if (len == 0) {
        return;
    }
    if (s_rx_mux && xSemaphoreTake(s_rx_mux, portMAX_DELAY) == pdTRUE) {
        rx_ring_push(data, len);
        xSemaphoreGive(s_rx_mux);
    }
    rx_notify_data_ready();
    if (s_rx_cb) {
        s_rx_cb(data, len, s_rx_cb_ctx);
    }
}

static void uart_rx_event_task(void *arg)
{
    uart_event_t event;
    uint8_t chunk[DRIVER_UART_EVT_READ_CHUNK];

    for (;;) {
        if (xQueueReceive(s_uart_queue, &event, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        switch (event.type) {
        case UART_DATA: {
            size_t to_read = event.size;
            while (to_read > 0) {
                size_t chunk_len = to_read > sizeof(chunk) ? sizeof(chunk) : to_read;
                int n = uart_read_bytes(UART0_PORT_NUM, chunk, chunk_len, 0);
                if (n <= 0) {
                    break;
                }
                uart_on_rx_bytes(chunk, (size_t)n);
                to_read -= (size_t)n;
            }
            break;
        }
        case UART_FIFO_OVF:
            ESP_LOGW(TAG, "RX FIFO overflow, flush HW buffer");
            uart_flush_input(UART0_PORT_NUM);
            xQueueReset(s_uart_queue);
            break;
        case UART_BUFFER_FULL:
            ESP_LOGW(TAG, "driver RX buffer full, drain HW");
            uart_flush_input(UART0_PORT_NUM);
            xQueueReset(s_uart_queue);
            break;
        case UART_BREAK:
            ESP_LOGW(TAG, "UART break");
            break;
        case UART_PARITY_ERR:
            ESP_LOGW(TAG, "UART parity error");
            break;
        case UART_FRAME_ERR:
            ESP_LOGW(TAG, "UART frame error");
            break;
        default:
            break;
        }
    }
}

static void uart_tx_task(void *arg)
{
    driver_uart_tx_pkt_t pkt;

    for (;;) {
        if (xQueueReceive(s_tx_queue, &pkt, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        if (pkt.len == 0) {
            continue;
        }
        int n = uart_write_bytes(UART0_PORT_NUM, pkt.data, pkt.len);
        if (n < 0 || (size_t)n != pkt.len) {
            ESP_LOGW(TAG, "TX short write %d/%u", n, (unsigned)pkt.len);
        }
    }
}

static int uart_flow_pin(int gpio)
{
    return (gpio == PIN_NC) ? UART_PIN_NO_CHANGE : gpio;
}

static void rx_sem_drain(void)
{
<<<<<<< HEAD
    if (s_rx_data_sem == NULL) 
    {
        return;
    }

    while (xSemaphoreTake(s_rx_data_sem, 0) == pdTRUE) 
    {
=======
    if (s_rx_data_sem == NULL) {
        return;
    }
    while (xSemaphoreTake(s_rx_data_sem, 0) == pdTRUE) {
>>>>>>> 5c0939b1a3ae63d96b8bb499f5e2b674e3bfe0bf
    }
}

esp_err_t driver_uart_init(void)
{
    if (s_inited)
    {
        return ESP_ERR_INVALID_STATE;
    }

    const uart_config_t cfg = 
    {
        .baud_rate = UART0_BAUD_RATE,
        .data_bits = UART0_DATA_BITS,
        .parity = UART0_PARITY,
        .stop_bits = UART0_STOP_BITS,
        .flow_ctrl = UART0_FLOW_CTRL,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_RETURN_ON_ERROR(uart_param_config(UART0_PORT_NUM, &cfg), TAG, "param");

    ESP_RETURN_ON_ERROR(
        uart_set_pin(UART0_PORT_NUM,
                     UART0_TX_GPIO,
                     UART0_RX_GPIO,
                     uart_flow_pin(UART0_RTS_GPIO),
                     uart_flow_pin(UART0_CTS_GPIO)),
        TAG,
        "set pin");

    ESP_RETURN_ON_ERROR(
        uart_driver_install(UART0_PORT_NUM,
                            UART0_RX_BUF_SIZE,
                            UART0_TX_BUF_SIZE,
                            DRIVER_UART_EVENT_QUEUE_LEN,
                            &s_uart_queue,
                            0),
        TAG,
        "install");

    s_rx_mux = xSemaphoreCreateMutex();
    s_rx_data_sem = xSemaphoreCreateCounting(DRIVER_UART_RX_NOTIFY_MAX, 0);
    s_tx_queue = xQueueCreate(DRIVER_UART_TX_QUEUE_DEPTH, sizeof(driver_uart_tx_pkt_t));
<<<<<<< HEAD

    if (s_rx_mux == NULL || s_rx_data_sem == NULL || s_tx_queue == NULL) 
    {
        if (s_tx_queue) 
        {
            vQueueDelete(s_tx_queue);
            s_tx_queue = NULL;
        }

        if (s_rx_data_sem) 
        {
            vSemaphoreDelete(s_rx_data_sem);
            s_rx_data_sem = NULL;
        }

        if (s_rx_mux) 
        {
            vSemaphoreDelete(s_rx_mux);
            s_rx_mux = NULL;
        }

=======
    if (s_rx_mux == NULL || s_rx_data_sem == NULL || s_tx_queue == NULL) {
        if (s_tx_queue) {
            vQueueDelete(s_tx_queue);
            s_tx_queue = NULL;
        }
        if (s_rx_data_sem) {
            vSemaphoreDelete(s_rx_data_sem);
            s_rx_data_sem = NULL;
        }
        if (s_rx_mux) {
            vSemaphoreDelete(s_rx_mux);
            s_rx_mux = NULL;
        }
>>>>>>> 5c0939b1a3ae63d96b8bb499f5e2b674e3bfe0bf
        uart_driver_delete(UART0_PORT_NUM);
        return ESP_ERR_NO_MEM;
    }

    s_rx_head = 0;
    s_rx_tail = 0;

    BaseType_t ok_rx = xTaskCreate(
        uart_rx_event_task,
        "uart_rx",
        DRIVER_UART_RX_TASK_STACK,
        NULL,
        DRIVER_UART_RX_TASK_PRIO,
        &s_rx_task);
    BaseType_t ok_tx = xTaskCreate(
        uart_tx_task,
        "uart_tx",
        DRIVER_UART_TX_TASK_STACK,
        NULL,
        DRIVER_UART_TX_TASK_PRIO,
        &s_tx_task);
<<<<<<< HEAD

    if (ok_rx != pdPASS || ok_tx != pdPASS) 
    {
        if (s_tx_task) 
        {
            vTaskDelete(s_tx_task);
            s_tx_task = NULL;
        }

        if (s_rx_task) 
        {
=======
    if (ok_rx != pdPASS || ok_tx != pdPASS) {
        if (s_tx_task) {
            vTaskDelete(s_tx_task);
            s_tx_task = NULL;
        }
        if (s_rx_task) {
>>>>>>> 5c0939b1a3ae63d96b8bb499f5e2b674e3bfe0bf
            vTaskDelete(s_rx_task);
            s_rx_task = NULL;
        }
        vQueueDelete(s_tx_queue);
        s_tx_queue = NULL;
        vSemaphoreDelete(s_rx_data_sem);
        s_rx_data_sem = NULL;
        vSemaphoreDelete(s_rx_mux);
        s_rx_mux = NULL;
        uart_driver_delete(UART0_PORT_NUM);
        return ESP_ERR_NO_MEM;
    }

    s_inited = true;
    ESP_LOGI(TAG, "UART%d TX=%d RX=%d baud=%d (RX=ISR/event, TX=task)",
             UART0_PORT_NUM, UART0_TX_GPIO, UART0_RX_GPIO, UART0_BAUD_RATE);
    return ESP_OK;
}

esp_err_t driver_uart_deinit(void)
{
<<<<<<< HEAD
    if (!s_inited) 
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_tx_task) 
    {
        vTaskDelete(s_tx_task);
        s_tx_task = NULL;
    }

    if (s_rx_task) 
    {
=======
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_tx_task) {
        vTaskDelete(s_tx_task);
        s_tx_task = NULL;
    }
    if (s_rx_task) {
>>>>>>> 5c0939b1a3ae63d96b8bb499f5e2b674e3bfe0bf
        vTaskDelete(s_rx_task);
        s_rx_task = NULL;
    }

    uart_driver_delete(UART0_PORT_NUM);
    s_uart_queue = NULL;

<<<<<<< HEAD
    if (s_tx_queue) 
    {
        vQueueDelete(s_tx_queue);
        s_tx_queue = NULL;
    }

    if (s_rx_data_sem) 
    {
        vSemaphoreDelete(s_rx_data_sem);
        s_rx_data_sem = NULL;
    }

    if (s_rx_mux) 
    {
=======
    if (s_tx_queue) {
        vQueueDelete(s_tx_queue);
        s_tx_queue = NULL;
    }
    if (s_rx_data_sem) {
        vSemaphoreDelete(s_rx_data_sem);
        s_rx_data_sem = NULL;
    }
    if (s_rx_mux) {
>>>>>>> 5c0939b1a3ae63d96b8bb499f5e2b674e3bfe0bf
        vSemaphoreDelete(s_rx_mux);
        s_rx_mux = NULL;
    }

    s_rx_head = 0;
    s_rx_tail = 0;
    s_rx_cb = NULL;
    s_rx_cb_ctx = NULL;
    s_inited = false;
    return ESP_OK;
}

bool driver_uart_is_inited(void)
{
    return s_inited;
}

esp_err_t driver_uart_write(const uint8_t *data, size_t len, size_t *bytes_written)
{
<<<<<<< HEAD
    if (!s_inited || data == NULL || len == 0) 
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (len > DRIVER_UART_TX_PKT_MAX) 
    {
=======
    if (!s_inited || data == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (len > DRIVER_UART_TX_PKT_MAX) {
>>>>>>> 5c0939b1a3ae63d96b8bb499f5e2b674e3bfe0bf
        return ESP_ERR_INVALID_SIZE;
    }

    driver_uart_tx_pkt_t pkt;
    pkt.len = len;
    memcpy(pkt.data, data, len);

<<<<<<< HEAD
    if (xQueueSend(s_tx_queue, &pkt, DRIVER_UART_TX_ENQUEUE_TICKS) != pdTRUE) 
    {
        return ESP_ERR_TIMEOUT;
    }

    if (bytes_written) 
    {
        *bytes_written = len;
    }

=======
    if (xQueueSend(s_tx_queue, &pkt, DRIVER_UART_TX_ENQUEUE_TICKS) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    if (bytes_written) {
        *bytes_written = len;
    }
>>>>>>> 5c0939b1a3ae63d96b8bb499f5e2b674e3bfe0bf
    return ESP_OK;
}

esp_err_t driver_uart_read(uint8_t *buf, size_t max_len, size_t *out_len, TickType_t timeout_ticks)
{
<<<<<<< HEAD
    if (!s_inited || buf == NULL || max_len == 0 || out_len == NULL) 
    {
=======
    if (!s_inited || buf == NULL || max_len == 0 || out_len == NULL) {
>>>>>>> 5c0939b1a3ae63d96b8bb499f5e2b674e3bfe0bf
        return ESP_ERR_INVALID_ARG;
    }

    *out_len = 0;

<<<<<<< HEAD
    if (s_rx_mux && xSemaphoreTake(s_rx_mux, portMAX_DELAY) == pdTRUE) 
    {
        size_t got = rx_ring_pop(buf, max_len);
        xSemaphoreGive(s_rx_mux);

        if (got > 0) 
        {
=======
    if (s_rx_mux && xSemaphoreTake(s_rx_mux, portMAX_DELAY) == pdTRUE) {
        size_t got = rx_ring_pop(buf, max_len);
        xSemaphoreGive(s_rx_mux);
        if (got > 0) {
>>>>>>> 5c0939b1a3ae63d96b8bb499f5e2b674e3bfe0bf
            *out_len = got;
            return ESP_OK;
        }
    }

<<<<<<< HEAD
    if (timeout_ticks == 0) 
    {
        return ESP_ERR_TIMEOUT;
    }

    if (xSemaphoreTake(s_rx_data_sem, timeout_ticks) != pdTRUE) 
    {
        return ESP_ERR_TIMEOUT;
    }

    if (s_rx_mux && xSemaphoreTake(s_rx_mux, portMAX_DELAY) == pdTRUE) 
    {
        size_t got = rx_ring_pop(buf, max_len);
        xSemaphoreGive(s_rx_mux);

        if (got > 0) 
        {
=======
    if (timeout_ticks == 0) {
        return ESP_ERR_TIMEOUT;
    }

    if (xSemaphoreTake(s_rx_data_sem, timeout_ticks) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    if (s_rx_mux && xSemaphoreTake(s_rx_mux, portMAX_DELAY) == pdTRUE) {
        size_t got = rx_ring_pop(buf, max_len);
        xSemaphoreGive(s_rx_mux);
        if (got > 0) {
>>>>>>> 5c0939b1a3ae63d96b8bb499f5e2b674e3bfe0bf
            *out_len = got;
            return ESP_OK;
        }
    }

    return ESP_ERR_TIMEOUT;
}

size_t driver_uart_rx_available(void)
{
    size_t n = 0;
<<<<<<< HEAD

    if (!s_inited || s_rx_mux == NULL) 
    {
        return 0;
    }

    if (xSemaphoreTake(s_rx_mux, portMAX_DELAY) == pdTRUE) 
    {
        n = rx_ring_used();
        xSemaphoreGive(s_rx_mux);
    }

=======
    if (!s_inited || s_rx_mux == NULL) {
        return 0;
    }
    if (xSemaphoreTake(s_rx_mux, portMAX_DELAY) == pdTRUE) {
        n = rx_ring_used();
        xSemaphoreGive(s_rx_mux);
    }
>>>>>>> 5c0939b1a3ae63d96b8bb499f5e2b674e3bfe0bf
    return n;
}

void driver_uart_flush_rx(void)
{
<<<<<<< HEAD
    if (!s_inited) 
    {
        return;
    }
    uart_flush_input(UART0_PORT_NUM);

    if (s_rx_mux && xSemaphoreTake(s_rx_mux, portMAX_DELAY) == pdTRUE) 
    {
=======
    if (!s_inited) {
        return;
    }
    uart_flush_input(UART0_PORT_NUM);
    if (s_rx_mux && xSemaphoreTake(s_rx_mux, portMAX_DELAY) == pdTRUE) {
>>>>>>> 5c0939b1a3ae63d96b8bb499f5e2b674e3bfe0bf
        s_rx_head = 0;
        s_rx_tail = 0;
        xSemaphoreGive(s_rx_mux);
    }
<<<<<<< HEAD

=======
>>>>>>> 5c0939b1a3ae63d96b8bb499f5e2b674e3bfe0bf
    rx_sem_drain();
}

esp_err_t driver_uart_set_rx_callback(driver_uart_rx_cb_t cb, void *user_ctx)
{
<<<<<<< HEAD
    if (!s_inited) 
    {
        return ESP_ERR_INVALID_STATE;
    }
    
=======
    if (!s_inited) {
        return ESP_ERR_INVALID_STATE;
    }
>>>>>>> 5c0939b1a3ae63d96b8bb499f5e2b674e3bfe0bf
    s_rx_cb = cb;
    s_rx_cb_ctx = user_ctx;
    return ESP_OK;
}
