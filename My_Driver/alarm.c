#include "alarm.h"
#include "beep.h"
#include "bt401.h"
#include "flash.h"
#include "gpio.h"
#include "hardware_register.h"
#include "key.h"
#include "led.h"
#include "main.h"
#include "register_interface.h"
#include "rtc.h"
#include "string.h"
#include <stdbool.h>

// 存储多个闹钟的数组
Alarm_struct alarms[MAX_ALARMS] = {0};
bool         ring_flag          = 0;
// static uint8_t globa_volume = 10; // 默认音量为10，最大15。

void parse_alarm_data(uint16_t value_H, uint16_t value_L, Alarm_struct* alarm)
{
    // 解析高位寄存器 (0x0003)
    alarm->minute   = value_H & 0x3F;           // 0-5 位表示分钟
    alarm->hour     = (value_H >> 6) & 0x1F;    // 6-10 位表示小时
    alarm->alarm_id = (value_H >> 11) & 0x1F;   // 11-15 位表示闹钟ID

    // 解析低位寄存器 (0x0004)
    alarm->enabled     = value_L & 0x01;          // 0 位表示是否启用
    alarm->repeat      = (value_L >> 1) & 0x01;   // 1 位表示重复类型
    alarm->weekdays    = (value_L >> 2) & 0x7F;   // 2-8 位表示星期标志位
    alarm->ringtone_id = (value_L >> 9) & 0x7F;   // 9-15 位表示铃声ID
}
void alarm_poll()
{
    static uint8_t last_day = 0xFF;
    Date_Struct    date;
    Time_Struct    time;

    // 获取当前时间和日期
    get_current_datetime(&date, &time);

    // 跨天重置
    if (date.day != last_day)
    {
        for (int i = 0; i < MAX_ALARMS; i++)
        {
            alarms[i].triggered_today = 0;
        }
        last_day = date.day;
    }

    // 遍历闹钟数组
    for (int i = 0; i < MAX_ALARMS; i++)
    {
        if (!alarms[i].enabled || alarms[i].triggered_today) continue;

        if (alarms[i].hour == time.hours && alarms[i].minute == time.minutes)
        {
            uint8_t weekday_bit = (date.weekday == 0) ? 6 : (date.weekday - 1);
            if (alarms[i].weekdays & (1 << weekday_bit))
            {
                ring_alarm(alarms[i].ringtone_id);
                alarms[i].triggered_today = 1;

                // 仅单次闹钟禁用
                if (alarms[i].repeat == 0)
                {
                    alarms[i].enabled = 0;
                    save_alarms();   // 立即保存
                }
            }
        }
    }
}

// 在Flash中存储的结构：闹钟数据 + CRC校验值
#define ALARMS_DATA_SIZE sizeof(alarms)                  // 闹钟数据部分长度
#define CRC_SIZE sizeof(uint16_t)                        // CRC校验值长度
#define TOTAL_STORE_SIZE (ALARMS_DATA_SIZE + CRC_SIZE)   // 总存储长度

void alarm_init(void)
{
    uint8_t  flash_buffer[TOTAL_STORE_SIZE];
    uint16_t stored_crc, computed_crc;

    // 从Flash读取数据
    flash_read(FLASH_ALARM_ADDR, flash_buffer, TOTAL_STORE_SIZE);

    // 提取CRC校验值（小端模式）
    stored_crc = *((uint16_t*)(flash_buffer + ALARMS_DATA_SIZE));

    // 计算数据部分CRC
    computed_crc = _calc_check_value(flash_buffer, ALARMS_DATA_SIZE);

    if (stored_crc == computed_crc)
    {
        memcpy(alarms, flash_buffer, ALARMS_DATA_SIZE);
        // beep_start(50, 5); // 启动蜂鸣器，提示闹钟加载成功
    }
    else
    {
        // CRC校验失败处理
        memset(alarms, 0, ALARMS_DATA_SIZE);   // 清空闹钟
        save_alarms();                         // 写入默认值
    }
}

// 保存闹钟（自动添加CRC）
void save_alarms()
{
    uint8_t  flash_buffer[TOTAL_STORE_SIZE];
    uint16_t crc_value;

    // 拷贝闹钟数据
    memcpy(flash_buffer, alarms, ALARMS_DATA_SIZE);

    // 计算并存储CRC
    crc_value = _calc_check_value(flash_buffer, ALARMS_DATA_SIZE);
    memcpy(flash_buffer + ALARMS_DATA_SIZE, &crc_value, CRC_SIZE);

    // 写入Flash
    flash_write(FLASH_ALARM_ADDR, flash_buffer, TOTAL_STORE_SIZE);
}

void delete_alarm(uint8_t index)
{
    // 删除闹钟逻辑
    memset(&alarms[index], 0, sizeof(Alarm_struct));
    save_alarms();
}


void ring_alarm(uint8_t ringtone_id)
{
    ring_flag = true;

    // 启动基础音频（5秒20%占空比）
    // beep_start(500, 20);
    mode_control(MUSIC_MODE);
    HAL_Delay(1000);
    // send_at_command("AT+CA00\r\n", 50);
    // 播放铃声
    AT_PRINTF("AT+AB%02d\r\n", ringtone_id);
    HAL_Delay(100);
}

void ring_Gradually_increase()
{
    static uint8_t current_volume = 0;
    if (!ring_flag)
    {
        current_volume = 0;   // 重置音量
        return;               // 如果没有响铃，则不执行渐强
    }

    if (current_volume <= 30)
    {
        current_volume++;
        AT_PRINTF("AT+CA%02d\r\n", current_volume);   // 设置音量
    }
}
