#include "app_history_store.h"

#include "driver_sd_tf.h"

#include "driver_co2.h"
#include "driver_hum.h"
#include "driver_rpm.h"
#include "driver_temp.h"
#include "sd_tf_config.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static const char *TAG = "hist_store";

#define HIST_MAGIC                  0x31545348U /* 'HST1' little-endian */
#define HIST_VERSION                1U
#define HIST_WALL_CLOCK_MIN         1577836800U /* 2020-01-01 */

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint16_t channel;
    uint32_t sample_interval_s;
    uint32_t reserved;
    uint8_t pad[16];
} hist_file_hdr_t;

typedef struct __attribute__((packed)) {
    uint32_t ts;
    float value;
    uint8_t flags;
    uint8_t pad[3];
} hist_record_t;

static const char *const s_channel_dir[APP_HIST_CH_COUNT] = {
    TF_HISTORY_SUB_CO2,
    TF_HISTORY_SUB_TEMP,
    TF_HISTORY_SUB_HUMI,
    TF_HISTORY_SUB_SPEED,
};

static SemaphoreHandle_t s_sd_mtx;
static bool s_ready;
static bool s_task_started;
static hist_record_t s_pending[APP_HIST_CH_COUNT][APP_HIST_FLUSH_EVERY_N];
static size_t s_pending_cnt[APP_HIST_CH_COUNT];
static uint32_t s_open_ymd[APP_HIST_CH_COUNT];
static FILE *s_fp[APP_HIST_CH_COUNT];

static uint32_t history_timestamp_now(void)
{
    const time_t t = time(NULL);
    if (t > (time_t)HIST_WALL_CLOCK_MIN) {
        return (uint32_t)t;
    }
    return (uint32_t)(esp_timer_get_time() / 1000000ULL);
}

static bool wall_clock_valid(void)
{
    return time(NULL) > (time_t)HIST_WALL_CLOCK_MIN;
}

static void build_channel_path(char *path, size_t path_sz, app_hist_channel_t ch, const char *leaf)
{
    snprintf(path,
             path_sz,
             "%s/%s/%s/%s",
             TF_VFS_MOUNT_POINT,
             TF_HISTORY_DIR_NAME,
             s_channel_dir[ch],
             leaf);
}

static esp_err_t mkdir_one(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        return ESP_OK;
    }
    if (mkdir(path, 0755) != 0) {
        ESP_LOGE(TAG, "mkdir %s failed errno=%d", path, errno);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t ensure_history_dirs(void)
{
    char path[96];

    snprintf(path, sizeof(path), "%s/%s", TF_VFS_MOUNT_POINT, TF_HISTORY_DIR_NAME);
    if (mkdir_one(path) != ESP_OK) {
        return ESP_FAIL;
    }

    for (int i = 0; i < (int)APP_HIST_CH_COUNT; i++) {
        snprintf(path,
                 sizeof(path),
                 "%s/%s/%s",
                 TF_VFS_MOUNT_POINT,
                 TF_HISTORY_DIR_NAME,
                 s_channel_dir[i]);
        if (mkdir_one(path) != ESP_OK) {
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

static void file_leaf_name(uint32_t ts, char *leaf, size_t leaf_sz)
{
    if (wall_clock_valid()) {
        const time_t t = (time_t)ts;
        struct tm tm_info;
        localtime_r(&t, &tm_info);
        snprintf(leaf,
                 leaf_sz,
                 "%04d%02d%02d.bin",
                 tm_info.tm_year + 1900,
                 tm_info.tm_mon + 1,
                 tm_info.tm_mday);
    } else {
        snprintf(leaf, leaf_sz, "%s", TF_HISTORY_FALLBACK_FILE);
    }
}

static uint32_t ymd_from_ts(uint32_t ts)
{
    if (!wall_clock_valid()) {
        return 0U;
    }
    const time_t t = (time_t)ts;
    struct tm tm_info;
    localtime_r(&t, &tm_info);
    return (uint32_t)((tm_info.tm_year + 1900) * 10000 + (tm_info.tm_mon + 1) * 100 + tm_info.tm_mday);
}

static esp_err_t write_file_header(FILE *fp, app_hist_channel_t ch)
{
    hist_file_hdr_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = HIST_MAGIC;
    hdr.version = HIST_VERSION;
    hdr.channel = (uint16_t)ch;
    hdr.sample_interval_s = APP_HIST_SAMPLE_INTERVAL_S;

    if (fwrite(&hdr, 1, sizeof(hdr), fp) != sizeof(hdr)) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t open_channel_file(app_hist_channel_t ch, uint32_t ts, bool create)
{
    char leaf[32];
    char path[128];
    file_leaf_name(ts, leaf, sizeof(leaf));

    const uint32_t ymd = ymd_from_ts(ts);
    if (s_fp[ch] != NULL) {
        if (ymd == 0U && strcmp(leaf, TF_HISTORY_FALLBACK_FILE) == 0) {
            return ESP_OK;
        }
        if (ymd != 0U && s_open_ymd[ch] == ymd) {
            return ESP_OK;
        }
        fflush(s_fp[ch]);
        fclose(s_fp[ch]);
        s_fp[ch] = NULL;
    }

    build_channel_path(path, sizeof(path), ch, leaf);

    struct stat st;
    const bool exists = (stat(path, &st) == 0 && S_ISREG(st.st_mode));

    if (!exists && !create) {
        return ESP_ERR_NOT_FOUND;
    }

    FILE *fp = fopen(path, exists ? "ab" : "wb");
    if (!fp) {
        ESP_LOGE(TAG, "fopen %s failed", path);
        return ESP_FAIL;
    }

    if (!exists) {
        if (write_file_header(fp, ch) != ESP_OK) {
            fclose(fp);
            return ESP_FAIL;
        }
    }

    s_fp[ch] = fp;
    s_open_ymd[ch] = ymd;
    return ESP_OK;
}

static bool hist_channel_enabled(app_hist_channel_t ch)
{
    switch (ch) {
    case APP_HIST_CH_CO2:
        return co2_struct.En;
    case APP_HIST_CH_TEMP:
        return temp_struct.En;
    case APP_HIST_CH_HUM:
        return hum_struct.En;
    case APP_HIST_CH_RPM:
        return rpm_struct.En;
    default:
        return false;
    }
}

static bool hist_any_channel_enabled(void)
{
    for (int ch = 0; ch < (int)APP_HIST_CH_COUNT; ch++) {
        if (hist_channel_enabled((app_hist_channel_t)ch)) {
            return true;
        }
    }
    return false;
}

static uint8_t sample_flags_for_channel(app_hist_channel_t ch)
{
    switch (ch) {
    case APP_HIST_CH_CO2:
        return co2_struct.En ? 0U : 1U;
    case APP_HIST_CH_TEMP:
        return temp_struct.En ? 0U : 1U;
    case APP_HIST_CH_HUM:
        return hum_struct.En ? 0U : 1U;
    case APP_HIST_CH_RPM:
        return rpm_struct.En ? 0U : 1U;
    default:
        return 1U;
    }
}

static float sample_value_for_channel(app_hist_channel_t ch)
{
    switch (ch) {
    case APP_HIST_CH_CO2:
        return co2_struct.En ? co2_struct.CO2_Real_Value : 0.0f;
    case APP_HIST_CH_TEMP:
        return temp_struct.En ? temp_struct.Temp_Real_Value : 0.0f;
    case APP_HIST_CH_HUM:
        return hum_struct.En ? hum_struct.Hum_Real_Value : 0.0f;
    case APP_HIST_CH_RPM:
        return rpm_struct.En ? rpm_struct.RPM_Real_Value : 0.0f;
    default:
        return 0.0f;
    }
}

static esp_err_t flush_channel(app_hist_channel_t ch)
{
    if (s_pending_cnt[ch] == 0) {
        return ESP_OK;
    }

    const uint32_t ts = s_pending[ch][0].ts;
    esp_err_t err = open_channel_file(ch, ts, true);
    if (err != ESP_OK || !s_fp[ch]) {
        return err != ESP_OK ? err : ESP_FAIL;
    }

    for (size_t i = 0; i < s_pending_cnt[ch]; i++) {
        const hist_record_t *r = &s_pending[ch][i];
        if (fwrite(r, 1, sizeof(*r), s_fp[ch]) != sizeof(*r)) {
            ESP_LOGE(TAG, "write ch=%d failed", (int)ch);
            return ESP_FAIL;
        }
    }
    fflush(s_fp[ch]);
    s_pending_cnt[ch] = 0;
    return ESP_OK;
}

static void enqueue_sample(app_hist_channel_t ch, uint32_t ts, float value, uint8_t flags)
{
    if (s_pending_cnt[ch] >= APP_HIST_FLUSH_EVERY_N) {
        (void)flush_channel(ch);
    }

    hist_record_t *r = &s_pending[ch][s_pending_cnt[ch]];
    r->ts = ts;
    r->value = value;
    r->flags = flags;
    memset(r->pad, 0, sizeof(r->pad));
    s_pending_cnt[ch]++;
}

static esp_err_t read_file_header(FILE *fp, hist_file_hdr_t *hdr_out)
{
    if (fseek(fp, 0, SEEK_SET) != 0) {
        return ESP_FAIL;
    }
    if (fread(hdr_out, 1, sizeof(*hdr_out), fp) != sizeof(*hdr_out)) {
        return ESP_FAIL;
    }
    if (hdr_out->magic != HIST_MAGIC || hdr_out->version != HIST_VERSION) {
        return ESP_ERR_INVALID_VERSION;
    }
    return ESP_OK;
}

static esp_err_t read_records_tail(const char *path,
                                   uint32_t want,
                                   float *values_out,
                                   uint32_t *ts_out,
                                   size_t *out_count)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return ESP_ERR_NOT_FOUND;
    }

    hist_file_hdr_t hdr;
    if (read_file_header(fp, &hdr) != ESP_OK) {
        fclose(fp);
        return ESP_ERR_INVALID_VERSION;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return ESP_FAIL;
    }
    long fsize = ftell(fp);
    if (fsize < (long)APP_HIST_FILE_HEADER_SIZE) {
        fclose(fp);
        return ESP_ERR_NOT_FOUND;
    }

    const long data_bytes = fsize - (long)APP_HIST_FILE_HEADER_SIZE;
    const long rec_cnt = data_bytes / (long)sizeof(hist_record_t);
    if (rec_cnt <= 0) {
        fclose(fp);
        return ESP_ERR_NOT_FOUND;
    }

    const long take = (rec_cnt > (long)want) ? (long)want : rec_cnt;
    const long start_idx = rec_cnt - take;

    if (fseek(fp, (long)APP_HIST_FILE_HEADER_SIZE + start_idx * (long)sizeof(hist_record_t), SEEK_SET) != 0) {
        fclose(fp);
        return ESP_FAIL;
    }

    size_t got = 0;
    for (long i = 0; i < take; i++) {
        hist_record_t rec;
        if (fread(&rec, 1, sizeof(rec), fp) != sizeof(rec)) {
            break;
        }
        values_out[got] = rec.value;
        ts_out[got] = rec.ts;
        got++;
    }
    fclose(fp);

    if (got == 0) {
        return ESP_ERR_NOT_FOUND;
    }

    *out_count = got;
    return ESP_OK;
}

static void build_dated_leaf(int day_offset, char *leaf, size_t leaf_sz)
{
    time_t t = time(NULL);
    t += (time_t)day_offset * 86400;
    struct tm tm_info;
    localtime_r(&t, &tm_info);
    snprintf(leaf,
             leaf_sz,
             "%04d%02d%02d.bin",
             tm_info.tm_year + 1900,
             tm_info.tm_mon + 1,
             tm_info.tm_mday);
}

static esp_err_t purge_old_files(app_hist_channel_t ch)
{
    if (!wall_clock_valid()) {
        return ESP_OK;
    }

    char dirpath[96];
    snprintf(dirpath,
             sizeof(dirpath),
             "%s/%s/%s",
             TF_VFS_MOUNT_POINT,
             TF_HISTORY_DIR_NAME,
             s_channel_dir[ch]);

    DIR *dir = opendir(dirpath);
    if (!dir) {
        return ESP_OK;
    }

    time_t now = time(NULL);
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.') {
            continue;
        }
        if (strcmp(ent->d_name, TF_HISTORY_FALLBACK_FILE) == 0) {
            continue;
        }
        if (strlen(ent->d_name) != 12 || strstr(ent->d_name, ".bin") == NULL) {
            continue;
        }

        int y = 0;
        int m = 0;
        int d = 0;
        if (sscanf(ent->d_name, "%4d%2d%2d.bin", &y, &m, &d) != 3) {
            continue;
        }

        struct tm tm_old = { 0 };
        tm_old.tm_year = y - 1900;
        tm_old.tm_mon = m - 1;
        tm_old.tm_mday = d;
        const time_t file_t = mktime(&tm_old);
        if (file_t <= 0) {
            continue;
        }
        const double age_days = difftime(now, file_t) / 86400.0;
        if (age_days > (double)APP_HIST_RETENTION_DAYS) {
            char fpath[160];
            snprintf(fpath, sizeof(fpath), "%s/%s", dirpath, ent->d_name);
            if (remove(fpath) == 0) {
                ESP_LOGI(TAG, "purged %s", fpath);
            }
        }
    }
    closedir(dir);
    return ESP_OK;
}

static void history_store_task(void *arg)
{
    (void)arg;
    uint32_t tick = 0;

    ESP_LOGI(TAG, "sampler task running (interval=%us); SD write when channel En=true)",
             (unsigned)APP_HIST_SAMPLE_INTERVAL_S);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(APP_HIST_SAMPLE_INTERVAL_S * 1000U));

        /* 上电默认 En=false，不写卡、不占 SD 总线（与动画读卡互斥） */
        if (!hist_any_channel_enabled()) {
            continue;
        }

        if (!driver_sd_tf_bus_lock(pdMS_TO_TICKS(200))) {
            continue;
        }
        if (!s_ready || xSemaphoreTake(s_sd_mtx, pdMS_TO_TICKS(500)) != pdTRUE) {
            driver_sd_tf_bus_unlock();
            continue;
        }

        const uint32_t ts = history_timestamp_now();
        for (int ch = 0; ch < (int)APP_HIST_CH_COUNT; ch++) {
            if (!hist_channel_enabled((app_hist_channel_t)ch)) {
                continue;
            }
            enqueue_sample((app_hist_channel_t)ch,
                           ts,
                           sample_value_for_channel((app_hist_channel_t)ch),
                           sample_flags_for_channel((app_hist_channel_t)ch));
            if (s_pending_cnt[ch] >= APP_HIST_FLUSH_EVERY_N) {
                (void)flush_channel((app_hist_channel_t)ch);
            }
        }

        tick++;
        if ((tick % 60U) == 0U) {
            for (int ch = 0; ch < (int)APP_HIST_CH_COUNT; ch++) {
                (void)purge_old_files((app_hist_channel_t)ch);
            }
        }

        xSemaphoreGive(s_sd_mtx);
        driver_sd_tf_bus_unlock();
    }
}

esp_err_t app_history_store_init(void)
{
    if (s_ready) {
        return ESP_OK;
    }

    if (!s_sd_mtx) {
        s_sd_mtx = xSemaphoreCreateMutex();
        if (!s_sd_mtx) {
            return ESP_ERR_NO_MEM;
        }
    }

    if (xSemaphoreTake(s_sd_mtx, pdMS_TO_TICKS(2000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t err = ensure_history_dirs();
    if (err == ESP_OK) {
        memset(s_pending_cnt, 0, sizeof(s_pending_cnt));
        memset(s_fp, 0, sizeof(s_fp));
        memset(s_open_ymd, 0, sizeof(s_open_ymd));
        s_ready = true;
        ESP_LOGI(TAG, "ready under %s/%s/", TF_VFS_MOUNT_POINT, TF_HISTORY_DIR_NAME);
    }

    xSemaphoreGive(s_sd_mtx);
    return err;
}

esp_err_t app_history_store_start(void)
{
    if (s_task_started) {
        return ESP_OK;
    }
    if (!s_ready) {
        return ESP_ERR_INVALID_STATE;
    }

    const BaseType_t ok = xTaskCreate(history_store_task, "hist_store", 4096, NULL, 3, NULL);
    if (ok != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    s_task_started = true;
    return ESP_OK;
}

bool app_history_store_is_ready(void)
{
    return s_ready;
}

esp_err_t app_history_load_recent(app_hist_channel_t ch,
                                  uint32_t max_points,
                                  float *values_out,
                                  uint32_t *ts_out,
                                  size_t *count_out)
{
    if (!s_ready || ch >= APP_HIST_CH_COUNT || !values_out || !ts_out || !count_out || max_points == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!driver_sd_tf_bus_lock(pdMS_TO_TICKS(5000))) {
        return ESP_ERR_TIMEOUT;
    }
    if (xSemaphoreTake(s_sd_mtx, pdMS_TO_TICKS(3000)) != pdTRUE) {
        driver_sd_tf_bus_unlock();
        return ESP_ERR_TIMEOUT;
    }

    for (size_t i = 0; i < APP_HIST_CH_COUNT; i++) {
        (void)flush_channel((app_hist_channel_t)i);
    }

    char path[128];
    esp_err_t err = ESP_ERR_NOT_FOUND;
    size_t total = 0;

    if (wall_clock_valid()) {
        char leaf[32];
        build_dated_leaf(0, leaf, sizeof(leaf));
        build_channel_path(path, sizeof(path), ch, leaf);
        err = read_records_tail(path, max_points, values_out, ts_out, &total);
    }

    if (total == 0) {
        build_channel_path(path, sizeof(path), ch, TF_HISTORY_FALLBACK_FILE);
        err = read_records_tail(path, max_points, values_out, ts_out, &total);
    }

    xSemaphoreGive(s_sd_mtx);
    driver_sd_tf_bus_unlock();

    *count_out = total;
    return (total > 0) ? ESP_OK : err;
}
