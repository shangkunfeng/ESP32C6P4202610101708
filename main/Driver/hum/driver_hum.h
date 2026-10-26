#ifndef __DRIVER_HUM_H__
#define __DRIVER_HUM_H__

#include <stdbool.h>
#include <stdio.h>

typedef struct {
    bool En;
    uint8_t Hysteresis; //回差
    uint8_t Alarm_High; //高报警上限
    uint8_t Alarm_Low;  //低报警上限
    uint8_t PID_P_Value; //比例系数
    uint8_t PID_I_Value; //积分系数
    uint8_t PID_D_Value; //微分系数
    uint8_t PID_Max_Value; //PID输出最大值
    uint16_t Run_Time_Hour; //运行时间小时
    uint16_t Run_Time_Minute; //运行时间分钟
    
    float Hum_Set_Value; //设定CO2浓度
    float Hum_Real_Value;
} hum_structCtrl_t;

volatile extern hum_structCtrl_t hum_struct;

#endif
