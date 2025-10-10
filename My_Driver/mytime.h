#ifndef __MYTIME_H__
#define __MYTIME_H__

#include "rtc.h"
#include "stm32f1xx_hal.h"
#include "time.h"

// 日期结构体
typedef struct
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t weekday;   // 星期几 (0: 周日, 1: 周一, ..., 6: 周六)
} Date_Struct;

// 时间结构体
typedef struct
{
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
} Time_Struct;

void get_current_datetime(Date_Struct* date, Time_Struct* time);
void print_current_datetime(void);

HAL_StatusTypeDef write_utc(uint32_t time);
uint32_t read_utc(void);
struct tm* XX_RTC_GetTime(void);
void XX_RTC_Init(void);

#endif
