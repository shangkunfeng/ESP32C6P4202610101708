#include "uart_proto_payload.h"

#include "protocol_config.h"
#include "uart_proto_info_table.h"

#include "driver_co2.h"
#include "driver_hum.h"
#include "driver_rpm.h"
#include "driver_temp.h"

#include <string.h>

static esp_err_t apply_co2_set(const uint8_t *payload, size_t len)
{
    if (len != sizeof(co2_proto_set_t)) {
        return ESP_ERR_INVALID_SIZE;
    }
    co2_proto_set_t wire;
    memcpy(&wire, payload, sizeof(wire));
    co2_struct.En = (wire.en != 0);
    co2_struct.CO2_Set_Value = wire.set_value;
    return ESP_OK;
}

static esp_err_t pack_co2_real(uint8_t *payload, size_t buf_len, size_t *out_len)
{
    if (buf_len < sizeof(co2_proto_real_t)) {
        return ESP_ERR_NO_MEM;
    }
    co2_proto_real_t wire = {
        .real_value = co2_struct.CO2_Real_Value,
    };
    memcpy(payload, &wire, sizeof(wire));
    *out_len = sizeof(wire);
    return ESP_OK;
}

static esp_err_t apply_temp_set(const uint8_t *payload, size_t len)
{
    if (len != sizeof(temp_proto_set_t)) {
        return ESP_ERR_INVALID_SIZE;
    }
    temp_proto_set_t wire;
    memcpy(&wire, payload, sizeof(wire));
    temp_struct.En = (wire.en != 0);
    temp_struct.Temp_Set_Value = wire.set_value;
    return ESP_OK;
}

static esp_err_t pack_temp_real(uint8_t *payload, size_t buf_len, size_t *out_len)
{
    if (buf_len < sizeof(temp_proto_real_t)) {
        return ESP_ERR_NO_MEM;
    }
    temp_proto_real_t wire = {
        .real_value = temp_struct.Temp_Real_Value,
    };
    memcpy(payload, &wire, sizeof(wire));
    *out_len = sizeof(wire);
    return ESP_OK;
}

static esp_err_t apply_hum_set(const uint8_t *payload, size_t len)
{
    if (len != sizeof(hum_proto_set_t)) {
        return ESP_ERR_INVALID_SIZE;
    }
    hum_proto_set_t wire;
    memcpy(&wire, payload, sizeof(wire));
    hum_struct.En = (wire.en != 0);
    hum_struct.Hum_Set_Value = wire.set_value;
    return ESP_OK;
}

static esp_err_t pack_hum_real(uint8_t *payload, size_t buf_len, size_t *out_len)
{
    if (buf_len < sizeof(hum_proto_real_t)) {
        return ESP_ERR_NO_MEM;
    }
    hum_proto_real_t wire = {
        .real_value = hum_struct.Hum_Real_Value,
    };
    memcpy(payload, &wire, sizeof(wire));
    *out_len = sizeof(wire);
    return ESP_OK;
}

static esp_err_t apply_rpm_set(const uint8_t *payload, size_t len)
{
    if (len != sizeof(rpm_proto_set_t)) {
        return ESP_ERR_INVALID_SIZE;
    }
    rpm_proto_set_t wire;
    memcpy(&wire, payload, sizeof(wire));
    rpm_struct.En = (wire.en != 0);
    rpm_struct.RPM_Set_Value = wire.set_value;
    return ESP_OK;
}

static esp_err_t pack_rpm_real(uint8_t *payload, size_t buf_len, size_t *out_len)
{
    if (buf_len < sizeof(rpm_proto_real_t)) {
        return ESP_ERR_NO_MEM;
    }
    rpm_proto_real_t wire = {
        .real_value = rpm_struct.RPM_Real_Value,
    };
    memcpy(payload, &wire, sizeof(wire));
    *out_len = sizeof(wire);
    return ESP_OK;
}

esp_err_t uart_proto_payload_pack_set(uint8_t info_id, bool en, uint8_t *payload, size_t buf_len, size_t *out_len)
{
    if (payload == NULL || out_len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const uart_proto_info_entry_t *ent =
        uart_proto_info_table_find(info_id, PROTO_INFO_CAT_EVENT);
    if (ent == NULL || ent->flow != UART_PROTO_FLOW_HOST_TO_DEVICE) {
        return ESP_ERR_NOT_FOUND;
    }
    if (buf_len < ent->expected_length) {
        return ESP_ERR_NO_MEM;
    }

    switch (info_id) {
    case PROTO_INFO_ID_CO2: {
        co2_proto_set_t wire = {
            .en = en ? 1u : 0u,
            .reserved = { 0, 0, 0 },
            .set_value = co2_struct.CO2_Set_Value,
        };
        memcpy(payload, &wire, sizeof(wire));
        *out_len = sizeof(wire);
        return ESP_OK;
    }
    case PROTO_INFO_ID_TEMP: {
        temp_proto_set_t wire = {
            .en = en ? 1u : 0u,
            .reserved = { 0, 0, 0 },
            .set_value = temp_struct.Temp_Set_Value,
        };
        memcpy(payload, &wire, sizeof(wire));
        *out_len = sizeof(wire);
        return ESP_OK;
    }
    case PROTO_INFO_ID_HUM: {
        hum_proto_set_t wire = {
            .en = en ? 1u : 0u,
            .reserved = { 0, 0, 0 },
            .set_value = hum_struct.Hum_Set_Value,
        };
        memcpy(payload, &wire, sizeof(wire));
        *out_len = sizeof(wire);
        return ESP_OK;
    }
    case PROTO_INFO_ID_RPM: {
        rpm_proto_set_t wire = {
            .en = en ? 1u : 0u,
            .reserved = { 0, 0, 0 },
            .set_value = rpm_struct.RPM_Set_Value,
        };
        memcpy(payload, &wire, sizeof(wire));
        *out_len = sizeof(wire);
        return ESP_OK;
    }
    default:
        return ESP_ERR_NOT_SUPPORTED;
    }
}

esp_err_t uart_proto_payload_apply_set(uint8_t info_id, const uint8_t *payload, size_t len)
{
    const uart_proto_info_entry_t *ent =
        uart_proto_info_table_find(info_id, PROTO_INFO_CAT_EVENT);
    if (ent == NULL || ent->flow != UART_PROTO_FLOW_HOST_TO_DEVICE) {
        return ESP_ERR_NOT_FOUND;
    }
    if (len != ent->expected_length) {
        return ESP_ERR_INVALID_SIZE;
    }

    switch (info_id) {
    case PROTO_INFO_ID_CO2:
        return apply_co2_set(payload, len);
    case PROTO_INFO_ID_TEMP:
        return apply_temp_set(payload, len);
    case PROTO_INFO_ID_HUM:
        return apply_hum_set(payload, len);
    case PROTO_INFO_ID_RPM:
        return apply_rpm_set(payload, len);
    default:
        return ESP_ERR_NOT_SUPPORTED;
    }
}

static esp_err_t apply_co2_real(const uint8_t *payload, size_t len)
{
    if (len != sizeof(co2_proto_real_t)) {
        return ESP_ERR_INVALID_SIZE;
    }
    co2_proto_real_t wire;
    memcpy(&wire, payload, sizeof(wire));
    co2_struct.CO2_Real_Value = wire.real_value;
    return ESP_OK;
}

static esp_err_t apply_temp_real(const uint8_t *payload, size_t len)
{
    if (len != sizeof(temp_proto_real_t)) {
        return ESP_ERR_INVALID_SIZE;
    }
    temp_proto_real_t wire;
    memcpy(&wire, payload, sizeof(wire));
    temp_struct.Temp_Real_Value = wire.real_value;
    return ESP_OK;
}

static esp_err_t apply_hum_real(const uint8_t *payload, size_t len)
{
    if (len != sizeof(hum_proto_real_t)) {
        return ESP_ERR_INVALID_SIZE;
    }
    hum_proto_real_t wire;
    memcpy(&wire, payload, sizeof(wire));
    hum_struct.Hum_Real_Value = wire.real_value;
    return ESP_OK;
}

static esp_err_t apply_rpm_real(const uint8_t *payload, size_t len)
{
    if (len != sizeof(rpm_proto_real_t)) {
        return ESP_ERR_INVALID_SIZE;
    }
    rpm_proto_real_t wire;
    memcpy(&wire, payload, sizeof(wire));
    rpm_struct.RPM_Real_Value = wire.real_value;
    return ESP_OK;
}

esp_err_t uart_proto_payload_apply_real(uint8_t info_id, const uint8_t *payload, size_t len)
{
    const uart_proto_info_entry_t *ent =
        uart_proto_info_table_find(info_id, PROTO_INFO_CAT_PERIOD_EVENT);
    if (ent == NULL || ent->flow != UART_PROTO_FLOW_DEVICE_TO_HOST) {
        return ESP_ERR_NOT_FOUND;
    }
    if (len != ent->expected_length) {
        return ESP_ERR_INVALID_SIZE;
    }

    switch (info_id) {
    case PROTO_INFO_ID_CO2:
        return apply_co2_real(payload, len);
    case PROTO_INFO_ID_TEMP:
        return apply_temp_real(payload, len);
    case PROTO_INFO_ID_HUM:
        return apply_hum_real(payload, len);
    case PROTO_INFO_ID_RPM:
        return apply_rpm_real(payload, len);
    default:
        return ESP_ERR_NOT_SUPPORTED;
    }
}

esp_err_t uart_proto_payload_pack_real(uint8_t info_id, uint8_t *payload, size_t buf_len, size_t *out_len)
{
    if (payload == NULL || out_len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const uart_proto_info_entry_t *ent =
        uart_proto_info_table_find(info_id, PROTO_INFO_CAT_PERIOD_EVENT);
    if (ent == NULL || ent->flow != UART_PROTO_FLOW_DEVICE_TO_HOST) {
        return ESP_ERR_NOT_FOUND;
    }
    if (buf_len < ent->expected_length) {
        return ESP_ERR_NO_MEM;
    }

    switch (info_id) {
    case PROTO_INFO_ID_CO2:
        return pack_co2_real(payload, buf_len, out_len);
    case PROTO_INFO_ID_TEMP:
        return pack_temp_real(payload, buf_len, out_len);
    case PROTO_INFO_ID_HUM:
        return pack_hum_real(payload, buf_len, out_len);
    case PROTO_INFO_ID_RPM:
        return pack_rpm_real(payload, buf_len, out_len);
    default:
        return ESP_ERR_NOT_SUPPORTED;
    }
}
