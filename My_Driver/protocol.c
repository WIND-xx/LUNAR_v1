#include "protocol.h"
#include "alarm.h"
#include "beep.h"
#include "bt401.h"
#include "flash.h"
#include "hardware_register.h"
#include "key.h"
#include "led.h"
#include "main.h"
#include "register_interface.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern Alarm_struct alarms[MAX_ALARMS];

// 通信协议常量定义
#define PROTOCOL_HEADER 0x01
#define CMD_READ_REGISTER 0x03
#define CMD_WRITE_REGISTER 0x10
#define MAX_PACKET_SIZE 20
#define BUFFER_SIZE 64
#define TIMEOUT_MS 100
#define CHECKSUM_LENGTH 2

// 静态缓冲区和状态变量
static uint8_t _Buffer[BUFFER_SIZE];
static uint16_t _BufferLen = 0;

// 辅助函数：将16位整数转换为两个字节
static inline void _from_uint16(uint16_t value, uint8_t* bytes)
{
    bytes[0] = (uint8_t)(value >> 8);
    bytes[1] = (uint8_t)(value & 0xFF);
}

// 辅助函数：将两个字节转换为16位整数
static inline uint16_t _to_uint16(const uint8_t* bytes)
{
    return (uint16_t)(bytes[0] << 8) | bytes[1];
}

// 发送数据包到蓝牙设备，自动分包处理
static void Bluetooth_Send_Packet(const uint8_t* data, uint16_t length)
{
    uint16_t offset = 0;
    while (length > 0)
    {
        uint16_t packet_length = (length > MAX_PACKET_SIZE) ? MAX_PACKET_SIZE : length;
        BT401_Write((uint8_t*)(data + offset), packet_length);
        offset += packet_length;
        length -= packet_length;
    }
}

// 发送命令包，自动添加头部和校验
static void _send_cmd(uint8_t cmd, uint8_t* data, uint16_t length)
{
    data[0] = PROTOCOL_HEADER;
    data[1] = cmd;
    _from_uint16(_calc_check_value(data, length), &data[length]);
    Bluetooth_Send_Packet(data, length + CHECKSUM_LENGTH);
}

// 处理读寄存器命令
static void _do_read_reg_cmd(uint16_t addr, uint16_t num)
{
    // 参数有效性检查
    if (addr < REFRENCE_REG || addr >= REG_COUNT || num <= 0 || addr + num > REG_COUNT)
    {
        return;
    }

    // 准备响应数据
    _Buffer[2]     = num * 2;   // 数据字节数
    uint8_t offset = 3;

    // 填充寄存器值
    while (num--)
    {
        _from_uint16(register_get_value((RegisterID)addr++), &_Buffer[offset]);
        offset += 2;
    }

    // 发送响应
    _send_cmd(CMD_READ_REGISTER, _Buffer, offset);
}

// 处理报警设置数据
static void _process_alarm(const uint8_t data[])
{
    uint16_t alarm_H = _to_uint16(data);
    uint16_t alarm_L = _to_uint16(data + 2);
    Alarm_struct temp_alarm;

    parse_alarm_data(alarm_H, alarm_L, &temp_alarm);

    if (temp_alarm.alarm_id < MAX_ALARMS)
    {
        alarms[temp_alarm.alarm_id] = temp_alarm;
    }
}

// 处理UTC时间戳
static void _process_utc_timestamp(const uint8_t data[])
{
    uint32_t utc_timestamp = ((uint32_t)_to_uint16(data) << 16) | _to_uint16(data + 2);
    write_utc(utc_timestamp + 28800);   // UTC+8时区转换
}

// 处理写寄存器命令
static void _do_write_reg_cmd(uint16_t addr, const uint8_t data[], uint8_t num)
{
    // 参数有效性检查
    if (num + addr > REG_COUNT || addr == REFRENCE_REG)
    {
        return;
    }

    // 处理每个寄存器写入
    while (num-- > 0)
    {
        uint16_t value = _to_uint16(data);

        if (addr >= REFRENCE_REG)
        {
            // 读写寄存器写入
            register_set_value((RegisterID)addr, value);
        } else
        {
            // 只写寄存器处理
            switch (addr)
            {
                case REG_ALARM_SET_HIGH:   // 闹钟
                    _process_alarm(data);
                    _send_cmd(CMD_WRITE_REGISTER, _Buffer, 6);
                    return;
                case REG_UTC_TIMESTAMP_HIGH:   // UTC时间戳
                    _process_utc_timestamp(data);
                    _send_cmd(CMD_WRITE_REGISTER, _Buffer, 6);
                    return;
                default: _do_reg_changed(addr, value); return;   // 其余只写寄存器
            }
        }

        // 处理下一个寄存器
        ++addr;
        data += 2;
    }

    // 发送写操作成功响应
    _send_cmd(CMD_WRITE_REGISTER, _Buffer, 6);
}

// 解析接收到的数据帧
static bool _decode()
{
    // 基本帧格式检查
    if (_Buffer[0] != PROTOCOL_HEADER || _BufferLen < 8)
    {
        return false;
    }

    // 根据命令码处理
    switch (_Buffer[1])
    {
        case CMD_READ_REGISTER:
            // 读寄存器命令处理
            if (_calc_check_value(&_Buffer[0], 6) != _to_uint16(&_Buffer[6]))
            {
                return false;   // 校验失败
            }
            _do_read_reg_cmd(_to_uint16(&_Buffer[2]), _to_uint16(&_Buffer[4]));
            break;

        case CMD_WRITE_REGISTER:
            // 写寄存器命令处理
            if (_BufferLen < _Buffer[6] + 9 || _to_uint16(&_Buffer[4]) * 2 != _Buffer[6])
            {
                return false;   // 数据长度不匹配
            }

            _BufferLen = _Buffer[6] + 7;   // 更新缓冲区长度为实际数据长度

            if (_calc_check_value(&_Buffer[0], _BufferLen) != _to_uint16(&_Buffer[_BufferLen]))
            {
                return false;   // 校验失败
            }

            _do_write_reg_cmd(_to_uint16(&_Buffer[2]), &_Buffer[7], _Buffer[6] / 2);
            break;

        default: return false;   // 未知命令
    }

    return true;
}

// 协议轮询函数，处理蓝牙数据接收和解析
void protocol_poll(void)
{
    static uint32_t _tick = 0;

    // 读取蓝牙数据
    uint16_t length = BT401_Read(&_Buffer[_BufferLen], sizeof(_Buffer) - _BufferLen);
    if (length)
    {
        _BufferLen += length;
        _tick = HAL_GetTick();
    }

    // 处理超时或缓冲区满的情况
    if (_BufferLen >= sizeof(_Buffer) || (_BufferLen && tick_timeout(_tick, TIMEOUT_MS)))
    {
        bool success = _decode();

        if (success)
        {
            // 成功解析，发出提示音
            beep_start(5, 2);
        } else
        {
            // 解析失败，回显数据用于调试
            BT401_Write(_Buffer, _BufferLen);
        }

        // 清空缓冲区准备下一次接收
        _BufferLen = 0;
    }
}

// 上传参考寄存器值
void upload_reg_value(void)
{
    static uint8_t cmd[17] = {0};
    cmd[0]                 = 0x01;
    cmd[1]                 = 0x03;
    cmd[2]                 = 12;

    uint8_t offset = 3;
    for (int i = 0; i < 6; i++)
    {
        _from_uint16(register_get_value((RegisterID)(REFRENCE_REG + i)), &cmd[offset]);
        offset += 2;
    }

    _send_cmd(0x03, cmd, offset);
}
