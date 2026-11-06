#include "app_network_url_probe.h"

#include "app_eth.h"

#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "net_url_probe";

#define PROBE_TASK_STACK   8192
#define PROBE_TIMEOUT_MS   10000

typedef struct {
    char url[160];
    app_network_url_probe_done_fn cb;
    void *user_data;
    bool success;
    int http_status;
} probe_ctx_t;

static volatile bool s_cancel;
static volatile bool s_busy;
static probe_ctx_t *s_pending_async;

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
        snprintf(tmp, sizeof(tmp), "http://%s", buf);
        strncpy(buf, tmp, len - 1);
        buf[len - 1] = '\0';
    }
    return true;
}

static bool http_probe_url(const char *url, int *out_status)
{
    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = PROBE_TIMEOUT_MS,
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
    return (err == ESP_OK && status >= 200 && status < 400);
}

static void probe_async_cb(void *param)
{
    probe_ctx_t *ctx = param;
    if (!ctx) {
        return;
    }

    if (!s_cancel && ctx->cb) {
        ctx->cb(ctx->user_data, ctx->success, ctx->http_status);
    }
    free(ctx);
    s_pending_async = NULL;
}

static void probe_task(void *arg)
{
    probe_ctx_t *ctx = arg;
    bool ok = false;
    int status = 0;

    if (!s_cancel && app_eth_is_link_up()) {
        ok = http_probe_url(ctx->url, &status);
    } else if (!app_eth_is_link_up()) {
        ESP_LOGW(TAG, "ethernet link down");
    }

    ctx->success = ok;
    ctx->http_status = status;

    if (s_cancel) {
        free(ctx);
        s_busy = false;
        vTaskDelete(NULL);
        return;
    }

    s_pending_async = ctx;
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

    strncpy(ctx->url, url, sizeof(ctx->url) - 1);
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

    ESP_LOGI(TAG, "probe start");
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
