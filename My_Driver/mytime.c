#include "mytime.h"
#include "bt401.h"
#include "main.h"

// 判断是否为闰年
static int is_leap_year(uint16_t year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

// 获取每月天数
static uint8_t days_in_month(uint16_t year, uint8_t month)
{
    const uint8_t days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && is_leap_year(year))
    {
        return 29;
    }
    return days[month - 1];
}

// 计算星期几 (1970-01-01 是星期四)
static uint8_t calculate_weekday(uint32_t days_since_epoch)
{
    return (days_since_epoch + 4) % 7;   // 1970年1月1日是星期四 (+4)
}

// 将时间戳转换为日期和时间
static void UTC2DateTime(Date_Struct* date, Time_Struct* time, uint32_t timestamp)
{
    // 1. 计算日期
    uint32_t days    = timestamp / 86400;   // 一天有 86400 秒
    uint32_t seconds = timestamp % 86400;

    date->year = 1970;   // UTC 时间从 1970 年开始
    while (1)
    {
        uint16_t days_in_year = is_leap_year(date->year) ? 366 : 365;
        if (days < days_in_year) break;
        days -= days_in_year;
        date->year++;
    }

    date->month = 1;
    while (1)
    {
        uint8_t days_in_cur_month = days_in_month(date->year, date->month);
        if (days < days_in_cur_month) break;
        days -= days_in_cur_month;
        date->month++;
    }

    date->day     = days + 1;
    date->weekday = calculate_weekday(timestamp / 86400);   // 计算星期

    // 2. 计算时间
    time->hours = seconds / 3600;
    seconds %= 3600;
    time->minutes = seconds / 60;
    time->seconds = seconds % 60;
}

// RTC已经被初始化的值 记录在RTC_BKP_DR1中
#define RTC_INIT_FLAG 0x2333

/**
 * @brief  进入RTC初始化模式
 * @param  hrtc  指向包含RTC配置信息的RTC_HandleTypeDef结构体的指针
 * @retval HAL status
 */
static HAL_StatusTypeDef RTC_EnterInitMode(RTC_HandleTypeDef* hrtc)
{
    uint32_t tickstart = 0U;

    tickstart = HAL_GetTick();
    /* 等待RTC处于INIT状态，如果到达Time out 则退出 */
    while ((hrtc->Instance->CRL & RTC_CRL_RTOFF) == (uint32_t)RESET)
    {
        if ((HAL_GetTick() - tickstart) > RTC_TIMEOUT_VALUE)
        {
            return HAL_TIMEOUT;
        }
    }

    /* 禁用RTC寄存器的写保护 */
    __HAL_RTC_WRITEPROTECTION_DISABLE(hrtc);

    return HAL_OK;
}

/**
 * @brief  退出RTC初始化模式
 * @param  hrtc   指向包含RTC配置信息的RTC_HandleTypeDef结构体的指针
 * @retval HAL status
 */
static HAL_StatusTypeDef RTC_ExitInitMode(RTC_HandleTypeDef* hrtc)
{
    uint32_t tickstart = 0U;

    /* 禁用RTC寄存器的写保护。 */
    __HAL_RTC_WRITEPROTECTION_ENABLE(hrtc);

    tickstart = HAL_GetTick();
    /* 等到RTC处于INIT状态，如果到达Time out 则退出 */
    while ((hrtc->Instance->CRL & RTC_CRL_RTOFF) == (uint32_t)RESET)
    {
        if ((HAL_GetTick() - tickstart) > RTC_TIMEOUT_VALUE)
        {
            return HAL_TIMEOUT;
        }
    }

    return HAL_OK;
}

/**
 * @brief  写入RTC_CNT寄存器中的时间计数器。
 * @param  hrtc   指向包含RTC配置信息的RTC_HandleTypeDef结构体的指针。
 * @param  TimeCounter: 写入RTC_CNT寄存器的计数器
 * @retval HAL status
 */
static HAL_StatusTypeDef RTC_WriteTimeCounter(RTC_HandleTypeDef* hrtc, uint32_t TimeCounter)
{
    HAL_StatusTypeDef status = HAL_OK;

    /* 进入RTC初始化模式 */
    if (RTC_EnterInitMode(hrtc) != HAL_OK)
    {
        status = HAL_ERROR;
    } else
    {
        /* 设置RTC计数器高位寄存器 */
        WRITE_REG(hrtc->Instance->CNTH, (TimeCounter >> 16U));
        /* 设置RTC计数器低位寄存器 */
        WRITE_REG(hrtc->Instance->CNTL, (TimeCounter & RTC_CNTL_RTC_CNT));

        /* 退出RTC初始化模式 */
        if (RTC_ExitInitMode(hrtc) != HAL_OK)
        {
            status = HAL_ERROR;
        }
    }

    return status;
}
/**
 * @brief  读取RTC_CNT寄存器中的时间计数器。
 * @param  hrtc   指向包含RTC配置信息的RTC_HandleTypeDef结构体的指针。
 * @retval 时间计数器
 */
static uint32_t RTC_ReadTimeCounter(RTC_HandleTypeDef* hrtc)
{
    uint16_t high1 = 0U, high2 = 0U, low = 0U;
    uint32_t timecounter = 0U;

    high1 = READ_REG(hrtc->Instance->CNTH & RTC_CNTH_RTC_CNT);
    low   = READ_REG(hrtc->Instance->CNTL & RTC_CNTL_RTC_CNT);
    high2 = READ_REG(hrtc->Instance->CNTH & RTC_CNTH_RTC_CNT);

    if (high1 != high2)
    {
        /* 当读取CNTL和CNTH寄存器期间计数器溢出时, 重新读取CNTL寄存器然后返回计数器值 */
        timecounter = (((uint32_t)high2 << 16U) | READ_REG(hrtc->Instance->CNTL & RTC_CNTL_RTC_CNT));
    } else
    {
        /* 当读取CNTL和CNTH寄存器期间没有计数器溢出, 计数器值等于第一次读取的CNTL和CNTH值 */
        timecounter = (((uint32_t)high1 << 16U) | low);
    }

    return timecounter;
}
HAL_StatusTypeDef write_utc(uint32_t time)
{
    return RTC_WriteTimeCounter(&hrtc, time);
}
uint32_t read_utc(void)
{
    return RTC_ReadTimeCounter(&hrtc);
}
/**
 * @brief  获取当前时间
 * @retval 当前时间的指针
 */

/**
 * @brief 设置RTC时间
 * @param time 时间
 * @retval HAL status
 */
static HAL_StatusTypeDef XX_RTC_SetTime(struct tm* time)
{
    uint32_t unixTime = mktime(time);
    return RTC_WriteTimeCounter(&hrtc, unixTime);
}

void XX_RTC_Init()
{
    uint32_t initFlag = HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1);
    if (initFlag == RTC_INIT_FLAG) return;
    if (HAL_RTC_Init(&hrtc) != HAL_OK)
    {
        Error_Handler();
    }
    struct tm time = {
        .tm_year = 2024 - 1900,
        .tm_mon  = 1 - 1,
        .tm_mday = 1,
        .tm_hour = 23,
        .tm_min  = 59,
        .tm_sec  = 55,
    };
    XX_RTC_SetTime(&time);
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, RTC_INIT_FLAG);
}

void get_current_datetime(Date_Struct* date, Time_Struct* time)
{
    uint32_t timestamp = RTC_ReadTimeCounter(&hrtc);
    UTC2DateTime(date, time, timestamp);
}

void print_current_datetime(void)
{
    Date_Struct date;
    Time_Struct time;
    get_current_datetime(&date, &time);
    const char* weekdays[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    DEBUG_PRINTF("Date: %04d-%02d-%02d (%s)\n", date.year, date.month, date.day, weekdays[date.weekday]);
    DEBUG_PRINTF("Time: %02d:%02d:%02d\n", time.hours, time.minutes, time.seconds);
    HAL_Delay(100);
}
