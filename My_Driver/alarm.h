#ifndef __ALARM_H_
#define __ALARM_H_
#include "rtc.h"
#include "stm32f1xx_hal.h"
#include <mytime.h>
typedef struct
{
    uint8_t minute : 6;     // 分钟，占6位
    uint8_t hour : 5;       // 小时，占5位（0-24小时制）
    uint8_t alarm_id : 5;   // 闹钟ID，占5位

    uint8_t enabled : 1;       // 是否启用，占1位（0表示关闭，1表示启用）
    uint8_t repeat : 1;        // 重复类型，占1位（0表示仅一次，1表示重复）
    uint8_t weekdays : 7;      // 星期标志位，占7位（表示周一至周日）
    uint8_t ringtone_id : 7;   // 铃声ID，占7位
    uint8_t triggered_today;   // 今日是否已触发
} Alarm_struct;

#define MAX_ALARMS 10
#include <stdbool.h>
extern bool ring_flag;
extern Alarm_struct alarms[MAX_ALARMS];

void parse_alarm_data(uint16_t value_H, uint16_t value_L, Alarm_struct* alarm);

void alarm_poll(void);

void alarm_init(void);

void save_alarms(void);

void delete_alarm(uint8_t index);

void ring_alarm(uint8_t ringtone_id);

void ring_Gradually_increase(void);

#endif
