#include "app_network_url_probe.h"

#include "app_eth.h"
#include "app_network_hmi.h"

#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "net_url_probe";

#define PROBE_TASK_STACK   10240
#define PROBE_TIMEOUT_MS   10000
#define PROBE_BODY_MAX     (8 * 1024)

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} probe_body_t;

typedef struct {
    char url[160];
    app_network_url_probe_done_fn cb;
    void *user_data;
    bool success;
    int http_status;
    bool hmi_parsed;
    app_network_hmi_view_t hmi;
    char *body;
} probe_ctx_t;

static volatile bool s_cancel;
static volatile bool s_busy;

static void trim_url(char *s)
{
    if (!s) {
        return;
    }
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) {
        s[--n] = '\0';
    }
    size_t start = 0;
    while (s[start] && isspace((unsigned char)s[start])) {
        start++;
    }
    if (start > 0) {
        memmove(s, s + start, strlen(s + start) + 1);
    }
}

static bool normalize_url(char *buf, size_t len)
{
    if (!buf || len < 8) {
        return false;
    }
    trim_url(buf);
    if (buf[0] == '\0') {
        return false;
    }
    if (strstr(buf, "://") == NULL) {
        char tmp[160];
        const int n = snprintf(tmp, sizeof(tmp), "http://%s", buf);
        if (n < 0 || (size_t)n >= sizeof(tmp) || (size_t)n >= len) {
            return false;
        }
        memcpy(buf, tmp, (size_t)n + 1);
    }
    return true;
}

static esp_err_t http_on_data(esp_http_client_event_t *evt)
{
    if (evt->event_id != HTTP_EVENT_ON_DATA || !evt->data || evt->data_len <= 0) {
        return ESP_OK;
    }

    probe_body_t *acc = evt->user_data;
    if (!acc || !acc->data) {
        return ESP_OK;
    }

    size_t copy = (size_t)evt->data_len;
    if (acc->len + copy >= acc->cap) {
        copy = acc->cap - 1 - acc->len;
    }
    if (copy > 0) {
        memcpy(acc->data + acc->len, evt->data, copy);
        acc->len += copy;
        acc->data[acc->len] = '\0';
    }
    return ESP_OK;
}

static bool http_fetch_url(const char *url, int *out_status, char *body, size_t body_cap, size_t *out_len)
{
    probe_body_t acc = {
        .data = body,
        .len = 0,
        .cap = body_cap,
    };
    if (body && body_cap > 0) {
        body[0] = '\0';
    }

    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = PROBE_TIMEOUT_MS,
        .event_handler = http_on_data,
        .user_data = &acc,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        return false;
    }

    esp_err_t err = esp_http_client_perform(client);
    const int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (out_status) {
        *out_status = status;
    }
    if (out_len) {
        *out_len = acc.len;
    }
    return (err == ESP_OK && status >= 200 && status < 400);
}

static void probe_async_cb(void *param)
{
    probe_ctx_t *ctx = param;
    if (!ctx) {
        return;
    }

    if (!s_cancel && ctx->cb) {
        ctx->cb(ctx->user_data,
                ctx->success,
                ctx->http_status,
                ctx->hmi_parsed ? &ctx->hmi : NULL,
                ctx->hmi_parsed);
    }
    free(ctx->body);
    free(ctx);
}

static void probe_task(void *arg)
{
    probe_ctx_t *ctx = arg;
    bool ok = false;
    int status = 0;
    size_t body_len = 0;

    ctx->body = malloc(PROBE_BODY_MAX);
    if (!ctx->body) {
        ESP_LOGE(TAG, "body alloc failed");
    } else if (!s_cancel && app_eth_is_link_up()) {
        ok = http_fetch_url(ctx->url, &status, ctx->body, PROBE_BODY_MAX, &body_len);
        if (ok) {
            ctx->hmi_parsed = app_network_hmi_parse(ctx->body, body_len, &ctx->hmi);
            if (!ctx->hmi_parsed) {
                ESP_LOGW(TAG, "response is not HMI JSON (len=%u)", (unsigned)body_len);
            }
        }
    } else if (!app_eth_is_link_up()) {
        ESP_LOGW(TAG, "ethernet link down");
    }

    ctx->success = ok;
    ctx->http_status = status;

    if (s_cancel) {
        free(ctx->body);
        free(ctx);
        s_busy = false;
        vTaskDelete(NULL);
        return;
    }

    lv_async_call(probe_async_cb, ctx);
    s_busy = false;
    vTaskDelete(NULL);
}

esp_err_t app_network_url_probe_start(const char *url, app_network_url_probe_done_fn cb, void *user_data)
{
    if (!url || !cb) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_busy) {
        return ESP_ERR_INVALID_STATE;
    }

    probe_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        return ESP_ERR_NO_MEM;
    }

    const int url_len = snprintf(ctx->url, sizeof(ctx->url), "%s", url);
    if (url_len < 0 || (size_t)url_len >= sizeof(ctx->url)) {
        free(ctx);
        return ESP_ERR_INVALID_ARG;
    }
    if (!normalize_url(ctx->url, sizeof(ctx->url))) {
        free(ctx);
        return ESP_ERR_INVALID_ARG;
    }

    ctx->cb = cb;
    ctx->user_data = user_data;
    s_cancel = false;
    s_busy = true;

    BaseType_t ok = xTaskCreate(probe_task, "net_url_probe", PROBE_TASK_STACK, ctx, 5, NULL);
    if (ok != pdPASS) {
        s_busy = false;
        free(ctx);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "fetch start");
    return ESP_OK;
}

void app_network_url_probe_cancel(void)
{
    s_cancel = true;
}

bool app_network_url_probe_busy(void)
{
    return s_busy;
}
