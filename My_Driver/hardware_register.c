#include "hardware_register.h"
#include "alarm.h"
#include "beep.h"
#include "bt401.h"
#include "flash.h"
#include "key.h"
#include "led.h"
#include "main.h"
#include "mytime.h"
#include "ntc.h"
#include "pid.h"
#include "register_interface.h"
#include "rtc.h"
#include "shortcut.h"
#include "tim.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

uint32_t remaining_seconds = 0;
bool heating_active        = false;
bool music_active          = false;

// 时间管理
uint32_t get_remaining_seconds(void)
{
    return remaining_seconds;
}
void set_remaining_seconds(uint32_t seconds)
{
    remaining_seconds = seconds;
}

// 加热状态
void set_heating_active(bool active)
{
    heating_active = active;
}
bool is_heating_active(void)
{
    return heating_active;
}

// 音乐状态
void set_music_active(bool active)
{
    music_active = active;
}
bool is_music_active(void)
{
    return music_active;
}

// 超时检测
bool tick_timeout(uint32_t start, uint32_t timeout)
{
    return (HAL_GetTick() - start) >= timeout;
}


// RF控制
void rf_switch(uint8_t state)
{
    led_set_mode(LED_RF, state ? LED_MODE_ON : LED_MODE_OFF, 0);
    set_heating_active(state);
}

void rf_level(uint8_t level)
{
    static const uint8_t temperatures[] = {35, 45, 55};
    if (level < sizeof(temperatures) / sizeof(temperatures[0])) set_target_temperature(temperatures[level]);
}

void rf_time(uint16_t min)
{
    set_remaining_seconds(min * 60);
    led_time_select(min);
}

void update_hardware_registers(const uint16_t* data, uint16_t num)
{
    rf_switch(0);
    rf_level(0);
    rf_time(0);
}

void stop_heating_task(void)
{
    register_set_value(REG_HEATING_STATUS, 0);
    set_heating_active(false);
}

void stop_music_task(void)
{
    set_music_active(false);
    mode_control(NONE_MODE);
}

// 去除0x00字符
static void remove_out_0x00_dat(char* in_buff, int data_len)
{
    int str_len = 0;
    for (int i = 0; i < data_len; i++)
        if (in_buff[i] != 0x00) in_buff[str_len++] = in_buff[i];
    in_buff[str_len] = '\0';
}

// 发送命令并读取响应
static bool send_command_and_read_response(const char* cmd, uint32_t timeout_ms, char* resp_buffer, size_t buffer_size)
{
    if (!cmd || !resp_buffer || buffer_size == 0) return false;
    BT401_Printf("%s\r\n", cmd);
    HAL_Delay(timeout_ms);
    uint16_t bytes_read = BT401_Read((uint8_t*)resp_buffer, buffer_size - 1);
    if (bytes_read == 0) return false;
    remove_out_0x00_dat(resp_buffer, bytes_read);
    resp_buffer[bytes_read] = '\0';
    return true;
}

// 解析带前缀的数值响应
static bool parse_value_from_response(const char* response, const char* prefix, uint8_t* value, uint8_t default_value)
{
    if (!response || !prefix || !value) return false;
    char* start_ptr = strstr(response, prefix);
    if (!start_ptr)
    {
        *value = default_value;
        return false;
    }
    char* end_ptr;
    long parsed_value = strtol(start_ptr + strlen(prefix), &end_ptr, 10);
    if (end_ptr == start_ptr + strlen(prefix))
    {
        *value = default_value;
        return false;
    }
    *value = (uint8_t)(parsed_value & 0xFF);
    return true;
}

// 发送AT命令
bool send_at_command(const char* cmd, uint32_t timeout_ms)
{
    char resp_buffer[64] = {0};
    if (!send_command_and_read_response(cmd, timeout_ms, resp_buffer, sizeof(resp_buffer))) return false;
    if (strstr(resp_buffer, "ER")) return false;
    return strstr(resp_buffer, "OK") != NULL;
}

// 查询BLE状态
int query_ble_status(void)
{
    static uint8_t ble_status          = BLE_STATUS_DEFAULT;
    char resp_buffer[MAX_RESPONSE_LEN] = {0};
    if (send_command_and_read_response("AT+TS", CMD_TIMEOUT_MS, resp_buffer, sizeof(resp_buffer)))
        parse_value_from_response(resp_buffer, "TS+", &ble_status, BLE_STATUS_DEFAULT);
    return ble_status;
}

// 查询BLE连接模式
uint8_t query_ble_cm(void)
{
    uint8_t ble_cm                     = 0;
    char resp_buffer[MAX_RESPONSE_LEN] = {0};
    if (send_command_and_read_response("AT+QM", CMD_TIMEOUT_MS, resp_buffer, sizeof(resp_buffer)))
        parse_value_from_response(resp_buffer, "QM+", &ble_cm, 0);
    return ble_cm;
}

// 查询音乐ID
uint8_t query_music_id(void)
{
    uint8_t music_id                   = MUSIC_ID_DEFAULT;
    char resp_buffer[MAX_RESPONSE_LEN] = {0};
    if (send_command_and_read_response("AT+M1", CMD_TIMEOUT_MS, resp_buffer, sizeof(resp_buffer)))
        parse_value_from_response(resp_buffer, "M1+", &music_id, MUSIC_ID_DEFAULT);
    // DEBUG_PRINTF("MusicID: %d\n", music_id);
    return music_id;
}
