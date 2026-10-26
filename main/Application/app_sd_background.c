#include "app_sd_background.h"

#include "display_config.h"
#include "sd_tf_config.h"

#include "esp_cache.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include <stdbool.h>
#include <stdint.h>

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static const char *TAG = "app_sd_bg";

#define FRAME_BYTES         ((size_t)LCD_H_RES * (size_t)LCD_V_RES * sizeof(uint16_t))
#define FRAME_PSRAM_ALIGN   ((size_t)128)

static uint16_t *s_ui_login;
static uint16_t *s_ui_home_back;
static uint16_t *s_ui_home;
static uint16_t *s_wifi_no_link;
static uint16_t *s_wifi_link;
static bool s_ui_login_ready;
static bool s_ui_home_back_ready;
static bool s_ui_home_ready;
static bool s_wifi_no_link_ready;
static bool s_wifi_link_ready;

static void log_dir(const char *dir_path)
{
    DIR *d = opendir(dir_path);
    if (!d) {
        ESP_LOGE(TAG, "opendir %s failed, errno=%d (%s)", dir_path, errno, strerror(errno));
        return;
    }
    ESP_LOGI(TAG, "listing %s:", dir_path);
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') {
            continue;
        }
        char full[160];
        snprintf(full, sizeof(full), "%s/%s", dir_path, ent->d_name);
        struct stat st;
        if (stat(full, &st) == 0) {
            ESP_LOGI(TAG, "  %s  (%ld bytes)", ent->d_name, (long)st.st_size);
        } else {
            ESP_LOGI(TAG, "  %s", ent->d_name);
        }
    }
    closedir(d);
}

static void path_for_ui_login(char *path, size_t path_len)
{
    snprintf(path, path_len, "%s/%s/%s",
             TF_VFS_MOUNT_POINT, TF_PHOTO_DIR_NAME, TF_UI_LOGIN_FILE);
}

static void path_for_ui_home_back(char *path, size_t path_len)
{
    snprintf(path, path_len, "%s/%s/%s",
             TF_VFS_MOUNT_POINT, TF_PHOTO_DIR_NAME, TF_UI_HOME_BACK_FILE);
}

static void path_for_ui_home(char *path, size_t path_len)
{
    snprintf(path, path_len, "%s/%s/%s",
             TF_VFS_MOUNT_POINT, TF_PHOTO_DIR_NAME, TF_UI_HOME_FILE);
}

static void path_for_wifi_no_link(char *path, size_t path_len)
{
    snprintf(path, path_len, "%s/%s/%s",
             TF_VFS_MOUNT_POINT, TF_IMAGES_DIR_NAME, TF_WIFI_NO_LINK_FILE);
}

static void path_for_wifi_link(char *path, size_t path_len)
{
    snprintf(path, path_len, "%s/%s/%s",
             TF_VFS_MOUNT_POINT, TF_IMAGES_DIR_NAME, TF_WIFI_LINK_FILE);
}

static uint16_t *alloc_frame_psram(const char *label)
{
    uint16_t *p = heap_caps_aligned_alloc(FRAME_PSRAM_ALIGN, FRAME_BYTES,
                                          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) {
        ESP_LOGE(TAG, "PSRAM alloc %s failed (%u bytes)", label, (unsigned)FRAME_BYTES);
        return NULL;
    }
    const uintptr_t addr = (uintptr_t)p;
    if ((addr % FRAME_PSRAM_ALIGN) != 0 || (FRAME_BYTES % FRAME_PSRAM_ALIGN) != 0) {
        ESP_LOGE(TAG, "PSRAM %s misaligned addr=%p size=%u", label, (void *)p, (unsigned)FRAME_BYTES);
        heap_caps_free(p);
        return NULL;
    }
    return p;
}

static void free_background_psram(void)
{
    if (s_ui_login) {
        heap_caps_free(s_ui_login);
        s_ui_login = NULL;
    }
    if (s_ui_home_back) {
        heap_caps_free(s_ui_home_back);
        s_ui_home_back = NULL;
    }
    if (s_ui_home) {
        heap_caps_free(s_ui_home);
        s_ui_home = NULL;
    }
    if (s_wifi_no_link) {
        heap_caps_free(s_wifi_no_link);
        s_wifi_no_link = NULL;
    }
    if (s_wifi_link) {
        heap_caps_free(s_wifi_link);
        s_wifi_link = NULL;
    }
    s_ui_login_ready = false;
    s_ui_home_back_ready = false;
    s_ui_home_ready = false;
    s_wifi_no_link_ready = false;
    s_wifi_link_ready = false;
}

static uint16_t *alloc_icon_psram(const char *label)
{
    uint16_t *p = heap_caps_aligned_alloc(FRAME_PSRAM_ALIGN, TF_WIFI_ICON_PSRAM_BYTES,
                                          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) {
        ESP_LOGE(TAG, "PSRAM alloc %s failed (%u bytes)", label, (unsigned)TF_WIFI_ICON_PSRAM_BYTES);
        return NULL;
    }
    memset(p, 0, TF_WIFI_ICON_PSRAM_BYTES);
    return p;
}

static esp_err_t load_icon_bin_from_sd(const char *path, void *dest)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGW(TAG, "icon open failed: %s (errno=%d %s)", path, errno, strerror(errno));
        return ESP_FAIL;
    }

    const size_t n = fread(dest, 1, TF_WIFI_ICON_BYTES, f);
    fclose(f);

    if (n != TF_WIFI_ICON_BYTES) {
        ESP_LOGW(TAG, "%s size %u, expect %u", path, (unsigned)n, (unsigned)TF_WIFI_ICON_BYTES);
        return ESP_ERR_INVALID_SIZE;
    }

    esp_err_t sync = esp_cache_msync(dest, TF_WIFI_ICON_PSRAM_BYTES,
                                     ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_TYPE_DATA);
    if (sync != ESP_OK) {
        ESP_LOGE(TAG, "cache msync %s: %s (addr=%p size=%u)", path, esp_err_to_name(sync), dest,
                 (unsigned)TF_WIFI_ICON_PSRAM_BYTES);
        return sync;
    }

    return ESP_OK;
}

static esp_err_t load_bin_from_sd(const char *path, void *dest)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "open failed: %s (errno=%d %s)", path, errno, strerror(errno));
        return ESP_FAIL;
    }

    const size_t n = fread(dest, 1, FRAME_BYTES, f);
    fclose(f);

    if (n != FRAME_BYTES) {
        ESP_LOGE(TAG, "%s size %u, expect %u", path, (unsigned)n, (unsigned)FRAME_BYTES);
        return ESP_ERR_INVALID_SIZE;
    }

    esp_err_t sync = esp_cache_msync(dest, FRAME_BYTES,
                                     ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_TYPE_DATA);
    if (sync != ESP_OK) {
        ESP_LOGE(TAG, "cache msync %s: %s (addr=%p)", path, esp_err_to_name(sync), dest);
        return sync;
    }

    return ESP_OK;
}

static esp_err_t probe_one_bin(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) {
        ESP_LOGE(TAG, "missing: %s (errno=%d %s)", path, errno, strerror(errno));
        return ESP_ERR_NOT_FOUND;
    }
    if ((size_t)st.st_size != FRAME_BYTES) {
        ESP_LOGW(TAG, "%s size %ld, expect %u RGB565 bytes",
                 path, (long)st.st_size, (unsigned)FRAME_BYTES);
    } else {
        ESP_LOGI(TAG, "ok: %s (%ld bytes)", path, (long)st.st_size);
    }
    return ESP_OK;
}

esp_err_t app_sd_background_probe(void)
{
    char photo_dir[96];
    snprintf(photo_dir, sizeof(photo_dir), "%s/%s", TF_VFS_MOUNT_POINT, TF_PHOTO_DIR_NAME);

    log_dir(TF_VFS_MOUNT_POINT);
    log_dir(photo_dir);

    int missing = 0;
    char path[128];

    path_for_ui_login(path, sizeof(path));
    if (probe_one_bin(path) != ESP_OK) {
        missing++;
    }

    path_for_ui_home_back(path, sizeof(path));
    if (probe_one_bin(path) != ESP_OK) {
        missing++;
    }

    path_for_ui_home(path, sizeof(path));
    if (probe_one_bin(path) != ESP_OK) {
        missing++;
    }

    if (missing > 0) {
        ESP_LOGE(TAG, "请在 PC 上把 %s、%s、%s 拷到盘符\\%s\\ 后安全弹出再重启",
                 TF_UI_LOGIN_FILE, TF_UI_HOME_BACK_FILE, TF_UI_HOME_FILE, TF_PHOTO_DIR_NAME);
        return ESP_ERR_NOT_FOUND;
    }
    return ESP_OK;
}

esp_err_t app_sd_background_load_psram(void)
{
    if (s_ui_login_ready && s_ui_home_back_ready && s_ui_home_ready) {
        return ESP_OK;
    }

    free_background_psram();

    char path[128];

    s_ui_login = alloc_frame_psram("ui_login");
    if (!s_ui_login) {
        free_background_psram();
        return ESP_ERR_NO_MEM;
    }
    path_for_ui_login(path, sizeof(path));
    esp_err_t login_err = load_bin_from_sd(path, s_ui_login);
    if (login_err != ESP_OK) {
        free_background_psram();
        return login_err;
    }
    ESP_LOGI(TAG, "cached ui_login %s -> PSRAM %p", path, (void *)s_ui_login);

    s_ui_home_back = alloc_frame_psram("ui_home_back");
    if (!s_ui_home_back) {
        free_background_psram();
        return ESP_ERR_NO_MEM;
    }
    path_for_ui_home_back(path, sizeof(path));
    esp_err_t home_err = load_bin_from_sd(path, s_ui_home_back);
    if (home_err != ESP_OK) {
        free_background_psram();
        return home_err;
    }
    ESP_LOGI(TAG, "cached ui_home_back %s -> PSRAM %p", path, (void *)s_ui_home_back);

    s_ui_home = alloc_frame_psram("ui_home");
    if (!s_ui_home) {
        free_background_psram();
        return ESP_ERR_NO_MEM;
    }
    path_for_ui_home(path, sizeof(path));
    esp_err_t ui_home_err = load_bin_from_sd(path, s_ui_home);
    if (ui_home_err != ESP_OK) {
        free_background_psram();
        return ui_home_err;
    }
    ESP_LOGI(TAG, "cached ui_home %s -> PSRAM %p", path, (void *)s_ui_home);

    s_ui_login_ready = true;
    s_ui_home_back_ready = true;
    s_ui_home_ready = true;

    path_for_wifi_no_link(path, sizeof(path));
    s_wifi_no_link = alloc_icon_psram("wifi_no_link");
    if (s_wifi_no_link && load_icon_bin_from_sd(path, s_wifi_no_link) == ESP_OK) {
        s_wifi_no_link_ready = true;
        ESP_LOGI(TAG, "cached %s -> PSRAM %p", path, (void *)s_wifi_no_link);
    } else if (s_wifi_no_link) {
        heap_caps_free(s_wifi_no_link);
        s_wifi_no_link = NULL;
    }

    path_for_wifi_link(path, sizeof(path));
    s_wifi_link = alloc_icon_psram("wifi_link");
    if (s_wifi_link && load_icon_bin_from_sd(path, s_wifi_link) == ESP_OK) {
        s_wifi_link_ready = true;
        ESP_LOGI(TAG, "cached %s -> PSRAM %p", path, (void *)s_wifi_link);
    } else if (s_wifi_link) {
        heap_caps_free(s_wifi_link);
        s_wifi_link = NULL;
    }

    ESP_LOGI(TAG, "ui_login + ui_home_back + ui_home in PSRAM (%u bytes/frame)",
             (unsigned)FRAME_BYTES);
    return ESP_OK;
}

bool app_sd_background_is_ready(void)
{
    return s_ui_login_ready && s_ui_home_back_ready && s_ui_home_ready;
}

const uint16_t *app_sd_background_get_ui_login_rgb565(void)
{
    if (!s_ui_login_ready || !s_ui_login) {
        return NULL;
    }
    return s_ui_login;
}

const uint16_t *app_sd_background_get_ui_home_back_rgb565(void)
{
    if (!s_ui_home_back_ready || !s_ui_home_back) {
        return NULL;
    }
    return s_ui_home_back;
}

const uint16_t *app_sd_background_get_ui_home_rgb565(void)
{
    if (!s_ui_home_ready || !s_ui_home) {
        return NULL;
    }
    return s_ui_home;
}

const uint16_t *app_sd_background_get_wifi_no_link_rgb565(void)
{
    if (!s_wifi_no_link_ready || !s_wifi_no_link) {
        return NULL;
    }
    return s_wifi_no_link;
}

const uint16_t *app_sd_background_get_wifi_link_rgb565(void)
{
    if (!s_wifi_link_ready || !s_wifi_link) {
        return NULL;
    }
    return s_wifi_link;
}
