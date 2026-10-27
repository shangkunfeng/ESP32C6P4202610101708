#include "uart_proto_info_table.h"

#include "protocol_config.h"
#include "protocol_payload.h"

#define UART_PROTO_REAL_PERIOD_MS    1000U

#define ENTRY(id, cat, flow_dir, len, period) \
    {                                         \
        .info_id = (id),                      \
        .info_cat = (cat),                    \
        .flow = (flow_dir),                   \
        .expected_length = (len),             \
        .period_ms = (period),                \
    }

/*
 * 4 通道 × 2 类 = 8 行（无整包 *_structCtrl_t、无纯周期 0x02 行）
 *
 * | info_id | info_cat | 说明           | flow              | payload              | period_ms |
 * |---------|----------|----------------|-------------------|----------------------|-----------|
 * | CO2     | EVENT    | 屏发启停+设定  | HOST_TO_DEVICE    | co2_proto_set_t      | 0         |
 * | CO2     | PER_EV   | 设备发实时浓度 | DEVICE_TO_HOST    | co2_proto_real_t     | 1000      |
 * | TEMP    | EVENT    | 屏发启停+设定  | HOST_TO_DEVICE    | temp_proto_set_t     | 0         |
 * | TEMP    | PER_EV   | 设备发实时温度 | DEVICE_TO_HOST    | temp_proto_real_t    | 1000      |
 * | HUM     | EVENT    | 屏发启停+设定  | HOST_TO_DEVICE    | hum_proto_set_t      | 0         |
 * | HUM     | PER_EV   | 设备发实时湿度 | DEVICE_TO_HOST    | hum_proto_real_t     | 1000      |
 * | RPM     | EVENT    | 屏发启停+设定  | HOST_TO_DEVICE    | rpm_proto_set_t      | 0         |
 * | RPM     | PER_EV   | 设备发实时转速 | DEVICE_TO_HOST    | rpm_proto_real_t     | 1000      |
 */
static const uart_proto_info_entry_t s_info_table[] = {
    ENTRY(PROTO_INFO_ID_CO2, PROTO_INFO_CAT_EVENT, UART_PROTO_FLOW_HOST_TO_DEVICE,
          sizeof(co2_proto_set_t), 0),
    ENTRY(PROTO_INFO_ID_CO2, PROTO_INFO_CAT_PERIOD_EVENT, UART_PROTO_FLOW_DEVICE_TO_HOST,
          sizeof(co2_proto_real_t), UART_PROTO_REAL_PERIOD_MS),

    ENTRY(PROTO_INFO_ID_TEMP, PROTO_INFO_CAT_EVENT, UART_PROTO_FLOW_HOST_TO_DEVICE,
          sizeof(temp_proto_set_t), 0),
    ENTRY(PROTO_INFO_ID_TEMP, PROTO_INFO_CAT_PERIOD_EVENT, UART_PROTO_FLOW_DEVICE_TO_HOST,
          sizeof(temp_proto_real_t), UART_PROTO_REAL_PERIOD_MS),

    ENTRY(PROTO_INFO_ID_HUM, PROTO_INFO_CAT_EVENT, UART_PROTO_FLOW_HOST_TO_DEVICE,
          sizeof(hum_proto_set_t), 0),
    ENTRY(PROTO_INFO_ID_HUM, PROTO_INFO_CAT_PERIOD_EVENT, UART_PROTO_FLOW_DEVICE_TO_HOST,
          sizeof(hum_proto_real_t), UART_PROTO_REAL_PERIOD_MS),

    ENTRY(PROTO_INFO_ID_RPM, PROTO_INFO_CAT_EVENT, UART_PROTO_FLOW_HOST_TO_DEVICE,
          sizeof(rpm_proto_set_t), 0),
    ENTRY(PROTO_INFO_ID_RPM, PROTO_INFO_CAT_PERIOD_EVENT, UART_PROTO_FLOW_DEVICE_TO_HOST,
          sizeof(rpm_proto_real_t), UART_PROTO_REAL_PERIOD_MS),
};

const uart_proto_info_entry_t *uart_proto_info_table_find(uint8_t info_id, uint8_t info_cat)
{
    for (size_t i = 0; i < sizeof(s_info_table) / sizeof(s_info_table[0]); i++) {
        if (s_info_table[i].info_id == info_id && s_info_table[i].info_cat == info_cat) {
            return &s_info_table[i];
        }
    }
    return NULL;
}

size_t uart_proto_info_table_count(void)
{
    return sizeof(s_info_table) / sizeof(s_info_table[0]);
}
