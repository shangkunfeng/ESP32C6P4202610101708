#ifndef __PROTOCOL_CONFIG_H__
#define __PROTOCOL_CONFIG_H__

/**
 * 串口应用层协议常量（帧格式见协议文档）。
 * 实现阶段在 uart_proto_frame / uart_proto_info_table 中使用。
 */

#define PROTO_FRAME_HEAD_0              0x55
#define PROTO_FRAME_HEAD_1              0xAA
#define PROTO_FRAME_TAIL_0              0xDD
#define PROTO_FRAME_TAIL_1              0xEE

#define PROTO_INFO_ID_CO2               0x01
#define PROTO_INFO_ID_TEMP              0x02
#define PROTO_INFO_ID_HUM               0x03
#define PROTO_INFO_ID_RPM               0x04

#define PROTO_INFO_CAT_EVENT            0x01  /** 屏→设备：启停 + 设定值（*_proto_set_t） */
#define PROTO_INFO_CAT_PERIOD           0x02  /** 保留，当前信息表未使用 */
#define PROTO_INFO_CAT_PERIOD_EVENT     0x03  /** 设备→屏：实时值（*_proto_real_t） */

#endif
