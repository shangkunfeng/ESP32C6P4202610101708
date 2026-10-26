#include "app_param_config.h"

#include "board_display.h"
#include "driver_co2.h"
#include "driver_hum.h"
#include "driver_rpm.h"
#include "driver_temp.h"
#include "sd_tf_config.h"

#include "cJSON.h"
#include "esp_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char *TAG = "param_cfg";

#define PARAM_MAGIC         "JC1060PARAM"
#define PARAM_VERSION       1
static char s_param_path[64];
static char s_param_tmp_path[72];

static void build_paths(void)
{
    snprintf(s_param_path, sizeof(s_param_path), "%s/%s/%s",
             TF_VFS_MOUNT_POINT, TF_PARAM_DIR_NAME, TF_PARAM_USER_FILE);
    snprintf(s_param_tmp_path, sizeof(s_param_tmp_path), "%s/%s/%s",
             TF_VFS_MOUNT_POINT, TF_PARAM_DIR_NAME, TF_PARAM_USER_TMP);
}

static uint8_t clamp_u8_from_double(double v)
{
    if (v < 0.0) {
        return 0;
    }
    if (v > 255.0) {
        return 255;
    }
    return (uint8_t)v;
}

static void apply_co2_defaults(void)
{
    co2_struct.En = false;
    co2_struct.Hysteresis = 5;
    co2_struct.Alarm_High = 120;
    co2_struct.Alarm_Low = 40;
    co2_struct.PID_P_Value = 10;
    co2_struct.PID_I_Value = 5;
    co2_struct.PID_D_Value = 1;
    co2_struct.PID_Max_Value = 100;
    co2_struct.Run_Time_Hour = 2;
    co2_struct.Run_Time_Minute = 0;
    co2_struct.CO2_Set_Value = 800.0f;
    co2_struct.CO2_Real_Value = 0.0f;
}

static void apply_temp_defaults(void)
{
    temp_struct.En = false;
    temp_struct.Hysteresis = 5;
    temp_struct.Alarm_High = 35;
    temp_struct.Alarm_Low = 15;
    temp_struct.PID_P_Value = 20;
    temp_struct.PID_I_Value = 8;
    temp_struct.PID_D_Value = 2;
    temp_struct.PID_Max_Value = 50;
    temp_struct.Run_Time_Hour = 1;
    temp_struct.Run_Time_Minute = 0;
    temp_struct.Temp_Set_Value = 25.0f;
    temp_struct.Temp_Real_Value = 0.0f;
}

static void apply_hum_defaults(void)
{
    hum_struct.En = false;
    hum_struct.Hysteresis = 2;
    hum_struct.Alarm_High = 80;
    hum_struct.Alarm_Low = 40;
    hum_struct.PID_P_Value = 15;
    hum_struct.PID_I_Value = 6;
    hum_struct.PID_D_Value = 0;
    hum_struct.PID_Max_Value = 80;
    hum_struct.Run_Time_Hour = 1;
    hum_struct.Run_Time_Minute = 30;
    hum_struct.Hum_Set_Value = 60.0f;
    hum_struct.Hum_Real_Value = 0.0f;
}

static void apply_rpm_defaults(void)
{
    rpm_struct.En = false;
    rpm_struct.Hysteresis = 50;
    rpm_struct.Alarm_High = 255;
    rpm_struct.Alarm_Low = 50;
    rpm_struct.PID_P_Value = 8;
    rpm_struct.PID_I_Value = 3;
    rpm_struct.PID_D_Value = 1;
    rpm_struct.PID_Max_Value = 200;
    rpm_struct.Run_Time_Hour = 0;
    rpm_struct.Run_Time_Minute = 30;
    rpm_struct.RPM_Set_Value = 1500.0f;
    rpm_struct.RPM_Real_Value = 0.0f;
}

void param_config_apply_factory_defaults(void)
{
    apply_co2_defaults();
    apply_temp_defaults();
    apply_hum_defaults();
    apply_rpm_defaults();
    board_set_backlight(100);
}

void param_config_stop_all_channels(void)
{
    co2_struct.En = false;
    temp_struct.En = false;
    hum_struct.En = false;
    rpm_struct.En = false;
}

static bool json_get_bool(const cJSON *obj, const char *key, bool *out)
{
    const cJSON *j = cJSON_GetObjectItem(obj, key);
    if (!j || !cJSON_IsBool(j)) {
        return false;
    }
    *out = cJSON_IsTrue(j);
    return true;
}

static bool json_get_u8(const cJSON *obj, const char *key, uint8_t *out)
{
    const cJSON *j = cJSON_GetObjectItem(obj, key);
    if (!j || !cJSON_IsNumber(j)) {
        return false;
    }
    *out = clamp_u8_from_double(cJSON_GetNumberValue(j));
    return true;
}

static bool json_get_u16(const cJSON *obj, const char *key, uint16_t *out)
{
    const cJSON *j = cJSON_GetObjectItem(obj, key);
    if (!j || !cJSON_IsNumber(j)) {
        return false;
    }
    double v = cJSON_GetNumberValue(j);
    if (v < 0.0) {
        v = 0.0;
    }
    if (v > 65535.0) {
        v = 65535.0;
    }
    *out = (uint16_t)v;
    return true;
}

static bool json_get_float(const cJSON *obj, const char *key, float *out)
{
    const cJSON *j = cJSON_GetObjectItem(obj, key);
    if (!j || !cJSON_IsNumber(j)) {
        return false;
    }
    *out = (float)cJSON_GetNumberValue(j);
    return true;
}

static bool json_load_u8(const cJSON *obj, const char *key, volatile uint8_t *dst)
{
    uint8_t v;
    if (!json_get_u8(obj, key, &v)) {
        return false;
    }
    *dst = v;
    return true;
}

static bool json_load_u16(const cJSON *obj, const char *key, volatile uint16_t *dst)
{
    uint16_t v;
    if (!json_get_u16(obj, key, &v)) {
        return false;
    }
    *dst = v;
    return true;
}

static bool json_load_float(const cJSON *obj, const char *key, volatile float *dst)
{
    float v;
    if (!json_get_float(obj, key, &v)) {
        return false;
    }
    *dst = v;
    return true;
}

static bool parse_co2(const cJSON *obj)
{
    if (!obj || !cJSON_IsObject(obj)) {
        return false;
    }
    bool ok = true;
    bool en = co2_struct.En;
    if (json_get_bool(obj, "En", &en)) {
        co2_struct.En = en;
    }
    ok &= json_load_u8(obj, "Hysteresis", &co2_struct.Hysteresis);
    ok &= json_load_u8(obj, "Alarm_High", &co2_struct.Alarm_High);
    ok &= json_load_u8(obj, "Alarm_Low", &co2_struct.Alarm_Low);
    ok &= json_load_u8(obj, "PID_P_Value", &co2_struct.PID_P_Value);
    ok &= json_load_u8(obj, "PID_I_Value", &co2_struct.PID_I_Value);
    ok &= json_load_u8(obj, "PID_D_Value", &co2_struct.PID_D_Value);
    ok &= json_load_u8(obj, "PID_Max_Value", &co2_struct.PID_Max_Value);
    ok &= json_load_u16(obj, "Run_Time_Hour", &co2_struct.Run_Time_Hour);
    ok &= json_load_u16(obj, "Run_Time_Minute", &co2_struct.Run_Time_Minute);
    ok &= json_load_float(obj, "CO2_Set_Value", &co2_struct.CO2_Set_Value);
    return ok;
}

static bool parse_temp(const cJSON *obj)
{
    if (!obj || !cJSON_IsObject(obj)) {
        return false;
    }
    bool en = temp_struct.En;
    if (json_get_bool(obj, "En", &en)) {
        temp_struct.En = en;
    }
    bool ok = json_load_u8(obj, "Hysteresis", &temp_struct.Hysteresis);
    ok &= json_load_u8(obj, "Alarm_High", &temp_struct.Alarm_High);
    ok &= json_load_u8(obj, "Alarm_Low", &temp_struct.Alarm_Low);
    ok &= json_load_u8(obj, "PID_P_Value", &temp_struct.PID_P_Value);
    ok &= json_load_u8(obj, "PID_I_Value", &temp_struct.PID_I_Value);
    ok &= json_load_u8(obj, "PID_D_Value", &temp_struct.PID_D_Value);
    ok &= json_load_u8(obj, "PID_Max_Value", &temp_struct.PID_Max_Value);
    ok &= json_load_u16(obj, "Run_Time_Hour", &temp_struct.Run_Time_Hour);
    ok &= json_load_u16(obj, "Run_Time_Minute", &temp_struct.Run_Time_Minute);
    ok &= json_load_float(obj, "Temp_Set_Value", &temp_struct.Temp_Set_Value);
    return ok;
}

static bool parse_hum(const cJSON *obj)
{
    if (!obj || !cJSON_IsObject(obj)) {
        return false;
    }
    bool en = hum_struct.En;
    if (json_get_bool(obj, "En", &en)) {
        hum_struct.En = en;
    }
    bool ok = json_load_u8(obj, "Hysteresis", &hum_struct.Hysteresis);
    ok &= json_load_u8(obj, "Alarm_High", &hum_struct.Alarm_High);
    ok &= json_load_u8(obj, "Alarm_Low", &hum_struct.Alarm_Low);
    ok &= json_load_u8(obj, "PID_P_Value", &hum_struct.PID_P_Value);
    ok &= json_load_u8(obj, "PID_I_Value", &hum_struct.PID_I_Value);
    ok &= json_load_u8(obj, "PID_D_Value", &hum_struct.PID_D_Value);
    ok &= json_load_u8(obj, "PID_Max_Value", &hum_struct.PID_Max_Value);
    ok &= json_load_u16(obj, "Run_Time_Hour", &hum_struct.Run_Time_Hour);
    ok &= json_load_u16(obj, "Run_Time_Minute", &hum_struct.Run_Time_Minute);
    ok &= json_load_float(obj, "Hum_Set_Value", &hum_struct.Hum_Set_Value);
    return ok;
}

static bool parse_rpm(const cJSON *obj)
{
    if (!obj || !cJSON_IsObject(obj)) {
        return false;
    }
    bool en = rpm_struct.En;
    if (json_get_bool(obj, "En", &en)) {
        rpm_struct.En = en;
    }
    bool ok = json_load_u8(obj, "Hysteresis", &rpm_struct.Hysteresis);
    ok &= json_load_u8(obj, "Alarm_High", &rpm_struct.Alarm_High);
    ok &= json_load_u8(obj, "Alarm_Low", &rpm_struct.Alarm_Low);
    ok &= json_load_u8(obj, "PID_P_Value", &rpm_struct.PID_P_Value);
    ok &= json_load_u8(obj, "PID_I_Value", &rpm_struct.PID_I_Value);
    ok &= json_load_u8(obj, "PID_D_Value", &rpm_struct.PID_D_Value);
    ok &= json_load_u8(obj, "PID_Max_Value", &rpm_struct.PID_Max_Value);
    ok &= json_load_u16(obj, "Run_Time_Hour", &rpm_struct.Run_Time_Hour);
    ok &= json_load_u16(obj, "Run_Time_Minute", &rpm_struct.Run_Time_Minute);
    ok &= json_load_float(obj, "RPM_Set_Value", &rpm_struct.RPM_Set_Value);
    return ok;
}

static cJSON *co2_to_json(void)
{
    cJSON *o = cJSON_CreateObject();
    if (!o) {
        return NULL;
    }
    cJSON_AddBoolToObject(o, "En", co2_struct.En);
    cJSON_AddNumberToObject(o, "Hysteresis", co2_struct.Hysteresis);
    cJSON_AddNumberToObject(o, "Alarm_High", co2_struct.Alarm_High);
    cJSON_AddNumberToObject(o, "Alarm_Low", co2_struct.Alarm_Low);
    cJSON_AddNumberToObject(o, "PID_P_Value", co2_struct.PID_P_Value);
    cJSON_AddNumberToObject(o, "PID_I_Value", co2_struct.PID_I_Value);
    cJSON_AddNumberToObject(o, "PID_D_Value", co2_struct.PID_D_Value);
    cJSON_AddNumberToObject(o, "PID_Max_Value", co2_struct.PID_Max_Value);
    cJSON_AddNumberToObject(o, "Run_Time_Hour", co2_struct.Run_Time_Hour);
    cJSON_AddNumberToObject(o, "Run_Time_Minute", co2_struct.Run_Time_Minute);
    cJSON_AddNumberToObject(o, "CO2_Set_Value", co2_struct.CO2_Set_Value);
    return o;
}

static cJSON *temp_to_json(void)
{
    cJSON *o = cJSON_CreateObject();
    if (!o) {
        return NULL;
    }
    cJSON_AddBoolToObject(o, "En", temp_struct.En);
    cJSON_AddNumberToObject(o, "Hysteresis", temp_struct.Hysteresis);
    cJSON_AddNumberToObject(o, "Alarm_High", temp_struct.Alarm_High);
    cJSON_AddNumberToObject(o, "Alarm_Low", temp_struct.Alarm_Low);
    cJSON_AddNumberToObject(o, "PID_P_Value", temp_struct.PID_P_Value);
    cJSON_AddNumberToObject(o, "PID_I_Value", temp_struct.PID_I_Value);
    cJSON_AddNumberToObject(o, "PID_D_Value", temp_struct.PID_D_Value);
    cJSON_AddNumberToObject(o, "PID_Max_Value", temp_struct.PID_Max_Value);
    cJSON_AddNumberToObject(o, "Run_Time_Hour", temp_struct.Run_Time_Hour);
    cJSON_AddNumberToObject(o, "Run_Time_Minute", temp_struct.Run_Time_Minute);
    cJSON_AddNumberToObject(o, "Temp_Set_Value", temp_struct.Temp_Set_Value);
    return o;
}

static cJSON *hum_to_json(void)
{
    cJSON *o = cJSON_CreateObject();
    if (!o) {
        return NULL;
    }
    cJSON_AddBoolToObject(o, "En", hum_struct.En);
    cJSON_AddNumberToObject(o, "Hysteresis", hum_struct.Hysteresis);
    cJSON_AddNumberToObject(o, "Alarm_High", hum_struct.Alarm_High);
    cJSON_AddNumberToObject(o, "Alarm_Low", hum_struct.Alarm_Low);
    cJSON_AddNumberToObject(o, "PID_P_Value", hum_struct.PID_P_Value);
    cJSON_AddNumberToObject(o, "PID_I_Value", hum_struct.PID_I_Value);
    cJSON_AddNumberToObject(o, "PID_D_Value", hum_struct.PID_D_Value);
    cJSON_AddNumberToObject(o, "PID_Max_Value", hum_struct.PID_Max_Value);
    cJSON_AddNumberToObject(o, "Run_Time_Hour", hum_struct.Run_Time_Hour);
    cJSON_AddNumberToObject(o, "Run_Time_Minute", hum_struct.Run_Time_Minute);
    cJSON_AddNumberToObject(o, "Hum_Set_Value", hum_struct.Hum_Set_Value);
    return o;
}

static cJSON *rpm_to_json(void)
{
    cJSON *o = cJSON_CreateObject();
    if (!o) {
        return NULL;
    }
    cJSON_AddBoolToObject(o, "En", rpm_struct.En);
    cJSON_AddNumberToObject(o, "Hysteresis", rpm_struct.Hysteresis);
    cJSON_AddNumberToObject(o, "Alarm_High", rpm_struct.Alarm_High);
    cJSON_AddNumberToObject(o, "Alarm_Low", rpm_struct.Alarm_Low);
    cJSON_AddNumberToObject(o, "PID_P_Value", rpm_struct.PID_P_Value);
    cJSON_AddNumberToObject(o, "PID_I_Value", rpm_struct.PID_I_Value);
    cJSON_AddNumberToObject(o, "PID_D_Value", rpm_struct.PID_D_Value);
    cJSON_AddNumberToObject(o, "PID_Max_Value", rpm_struct.PID_Max_Value);
    cJSON_AddNumberToObject(o, "Run_Time_Hour", rpm_struct.Run_Time_Hour);
    cJSON_AddNumberToObject(o, "Run_Time_Minute", rpm_struct.Run_Time_Minute);
    cJSON_AddNumberToObject(o, "RPM_Set_Value", rpm_struct.RPM_Set_Value);
    return o;
}

static esp_err_t ensure_param_dir(void)
{
    char dir[48];
    snprintf(dir, sizeof(dir), "%s/%s", TF_VFS_MOUNT_POINT, TF_PARAM_DIR_NAME);
    struct stat st;
    if (stat(dir, &st) == 0 && S_ISDIR(st.st_mode)) {
        return ESP_OK;
    }
    if (mkdir(dir, 0755) != 0) {
        ESP_LOGE(TAG, "mkdir %s failed", dir);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t param_config_load(void)
{
    build_paths();
    param_config_apply_factory_defaults();

    FILE *fp = fopen(s_param_path, "rb");
    if (!fp) {
        ESP_LOGI(TAG, "no %s, use factory defaults", s_param_path);
        return ESP_OK;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return ESP_OK;
    }
    long sz = ftell(fp);
    if (sz <= 0 || sz > 32768) {
        fclose(fp);
        ESP_LOGW(TAG, "param file size invalid, use defaults");
        return ESP_OK;
    }
    rewind(fp);

    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) {
        fclose(fp);
        return ESP_ERR_NO_MEM;
    }
    size_t rd = fread(buf, 1, (size_t)sz, fp);
    fclose(fp);
    buf[rd] = '\0';

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) {
        ESP_LOGW(TAG, "JSON parse failed, use defaults");
        return ESP_OK;
    }

    const cJSON *magic = cJSON_GetObjectItem(root, "magic");
    const cJSON *ver = cJSON_GetObjectItem(root, "version");
    if (!cJSON_IsString(magic) || strcmp(magic->valuestring, PARAM_MAGIC) != 0 ||
        !cJSON_IsNumber(ver) || (int)cJSON_GetNumberValue(ver) != PARAM_VERSION) {
        ESP_LOGW(TAG, "magic/version mismatch, use defaults");
        cJSON_Delete(root);
        return ESP_OK;
    }

    bool ok = parse_co2(cJSON_GetObjectItem(root, "co2"));
    ok &= parse_temp(cJSON_GetObjectItem(root, "temp"));
    ok &= parse_hum(cJSON_GetObjectItem(root, "hum"));
    ok &= parse_rpm(cJSON_GetObjectItem(root, "rpm"));

    const cJSON *bl = cJSON_GetObjectItem(root, "screen_brightness");
    if (cJSON_IsNumber(bl)) {
        uint32_t pct = (uint32_t)cJSON_GetNumberValue(bl);
        if (pct > 100) {
            pct = 100;
        }
        board_set_backlight(pct); /* clamps to LCD_BL_MIN_PERCENT */
    }

    cJSON_Delete(root);

    if (!ok) {
        ESP_LOGW(TAG, "partial param parse, revert to factory");
        param_config_apply_factory_defaults();
        return ESP_OK;
    }

    param_config_stop_all_channels();
    ESP_LOGI(TAG, "loaded %s (channels stopped until enabled in UI)", s_param_path);
    return ESP_OK;
}

esp_err_t param_config_save_to_sd(void)
{
    build_paths();
    esp_err_t err = ensure_param_dir();
    if (err != ESP_OK) {
        return err;
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddStringToObject(root, "magic", PARAM_MAGIC);
    cJSON_AddNumberToObject(root, "version", PARAM_VERSION);

    cJSON *co2 = co2_to_json();
    cJSON *temp = temp_to_json();
    cJSON *hum = hum_to_json();
    cJSON *rpm = rpm_to_json();
    if (!co2 || !temp || !hum || !rpm) {
        cJSON_Delete(co2);
        cJSON_Delete(temp);
        cJSON_Delete(hum);
        cJSON_Delete(rpm);
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddItemToObject(root, "co2", co2);
    cJSON_AddItemToObject(root, "temp", temp);
    cJSON_AddItemToObject(root, "hum", hum);
    cJSON_AddItemToObject(root, "rpm", rpm);
    cJSON_AddNumberToObject(root, "screen_brightness", (double)board_get_backlight());

    char *text = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!text) {
        return ESP_ERR_NO_MEM;
    }

    FILE *fp = fopen(s_param_tmp_path, "wb");
    if (!fp) {
        free(text);
        ESP_LOGE(TAG, "open %s failed", s_param_tmp_path);
        return ESP_FAIL;
    }
    size_t len = strlen(text);
    if (fwrite(text, 1, len, fp) != len) {
        fclose(fp);
        free(text);
        remove(s_param_tmp_path);
        return ESP_FAIL;
    }
    fclose(fp);
    free(text);

    remove(s_param_path);
    if (rename(s_param_tmp_path, s_param_path) != 0) {
        ESP_LOGE(TAG, "rename to %s failed", s_param_path);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "saved %s", s_param_path);
    return ESP_OK;
}

esp_err_t param_config_remove_user_file(void)
{
    build_paths();
    if (remove(s_param_path) == 0) {
        ESP_LOGI(TAG, "removed %s", s_param_path);
    }
    return ESP_OK;
}
