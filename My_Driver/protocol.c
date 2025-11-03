#include "protocol.h"
#include "alarm.h"
#include "beep.h"
#include "bt401.h"
#include "crc16.h"
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

// 发送命令包，内部构建完整帧（头部+命令+数据+校验），避免修改外部缓冲区
static void _send_cmd(uint8_t cmd, const uint8_t* data, uint16_t data_len)
{
    uint8_t frame[BUFFER_SIZE];                                     // 内部帧缓冲区，避免覆盖外部数据
    if (data_len + 1 + 1 + CHECKSUM_LENGTH > BUFFER_SIZE) return;   // 避免帧溢出

    // 构建帧头部
    frame[0] = PROTOCOL_HEADER;
    frame[1] = cmd;

    // 复制数据部分
    memcpy(&frame[2], data, data_len);

    // 计算并添加校验和
    uint16_t checksum = _calc_check_value(frame, 2 + data_len);   // 校验范围：头部+命令+数据
    _from_uint16(checksum, &frame[2 + data_len]);

    // 发送完整帧
    Bluetooth_Send_Packet(frame, 2 + data_len + CHECKSUM_LENGTH);
}

// 处理读寄存器命令
static void _do_read_reg_cmd(uint16_t addr, uint16_t num)
{
    // 参数有效性检查
    if (addr < REFRENCE_REG || addr >= REG_COUNT || num <= 0 || addr + num > REG_COUNT) return;

    // 准备响应数据（格式：[数据字节数][寄存器值1][寄存器值2]...）
    uint8_t resp_data[BUFFER_SIZE - 2];   // 预留头部和命令的位置
    uint8_t resp_len = 1;                 // 首字节为数据字节数
    resp_data[0]     = num * 2;           // 数据字节数 = 寄存器数量 * 2

    // 填充寄存器值
    for (uint16_t i = 0; i < num; i++)
    {
        if (resp_len + 2 > sizeof(resp_data)) return;   // 避免溢出
        _from_uint16(register_get_value((RegisterID)(addr + i)), &resp_data[resp_len]);
        resp_len += 2;
    }

    // 发送响应（命令=读命令，数据=响应数据）
    _send_cmd(CMD_READ_REGISTER, resp_data, resp_len);
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
    if (num == 0 || addr + num > REG_COUNT) return;

    // 准备写响应数据（格式：[地址高8位][地址低8位][数量高8位][数量低8位]）
    uint8_t resp_data[4];
    _from_uint16(addr, resp_data);
    _from_uint16(num, resp_data + 2);

    // 处理每个寄存器写入
    for (uint8_t i = 0; i < num; i++)
    {
        uint16_t value    = _to_uint16(data + 2 * i);
        RegisterID reg_id = (RegisterID)(addr + i);

        if (reg_id >= REFRENCE_REG)
        {
            // 读写寄存器写入
            register_set_value(reg_id, value);
        } else
        {
            // 只写寄存器处理
            switch (reg_id)
            {
                case REG_ALARM_SET_HIGH:   // 闹钟
                    _process_alarm(data + 2 * i);
                    _send_cmd(CMD_WRITE_REGISTER, resp_data, 4);   // 发送写响应
                    return;
                case REG_UTC_TIMESTAMP_HIGH:   // UTC时间戳
                    _process_utc_timestamp(data + 2 * i);
                    _send_cmd(CMD_WRITE_REGISTER, resp_data, 4);   // 发送写响应
                    return;
                default:
                {
                    _do_reg_changed(reg_id, value);
                    _send_cmd(CMD_WRITE_REGISTER, resp_data, 4);   // 发送写响应
                }

                    return;
            }
        }
    }

    // 所有寄存器写入完成后发送响应
    _send_cmd(CMD_WRITE_REGISTER, resp_data, 4);
}

// 解析接收到的数据帧
static bool _decode()
{
    // 基本帧格式检查（至少包含：头部+命令+校验）
    if (_BufferLen < 2 + CHECKSUM_LENGTH) return false;

    // 头部检查
    if (_Buffer[0] != PROTOCOL_HEADER) return false;

    uint8_t cmd  = _Buffer[1];
    bool success = false;

    switch (cmd)
    {
        case CMD_READ_REGISTER:
        {
            // 读命令格式：[头部(1)][命令(1)][地址(2)][数量(2)][校验(2)] → 总长度8
            if (_BufferLen < 6 + CHECKSUM_LENGTH) return false;

            // 校验和检查
            uint16_t recv_check = _to_uint16(&_Buffer[6]);
            uint16_t calc_check = _calc_check_value(_Buffer, 6);
            if (recv_check != calc_check) return false;

            // 解析地址和数量并处理
            uint16_t addr = _to_uint16(&_Buffer[2]);
            uint16_t num  = _to_uint16(&_Buffer[4]);
            _do_read_reg_cmd(addr, num);
            success = true;
            break;
        }   // 代码块结束

        case CMD_WRITE_REGISTER:
        {
            // 写命令格式：[头部(1)][命令(1)][地址(2)][数量(2)][数据长度(1)][数据(n)][校验(2)]
            if (_BufferLen < 7 + CHECKSUM_LENGTH)   // 最小长度：7（头部到数据长度）+2（校验）
                return false;

            uint8_t data_len   = _Buffer[6];
            uint16_t total_len = 7 + data_len + CHECKSUM_LENGTH;   // 总帧长
            if (_BufferLen < total_len)                            // 数据未接收完整
                return false;

            // 校验和检查
            uint16_t recv_check = _to_uint16(&_Buffer[7 + data_len]);
            uint16_t calc_check = _calc_check_value(_Buffer, 7 + data_len);
            if (recv_check != calc_check) return false;

            // 解析地址和数量并处理（数量=数据长度/2，每个寄存器2字节）
            uint16_t addr    = _to_uint16(&_Buffer[2]);
            uint16_t num_reg = data_len / 2;
            if (num_reg == 0 || data_len % 2 != 0)   // 数据长度必须为偶数
                return false;

            _do_write_reg_cmd(addr, &_Buffer[7], num_reg);
            success = true;
            break;
        }   // 代码块结束

        default: return false;   // 未知命令
    }

    return success;
}

// 协议轮询函数，处理蓝牙数据接收和解析
void protocol_poll(void)
{
    static uint32_t _tick = 0;

    // 读取蓝牙数据（避免缓冲区溢出）
    uint16_t available = sizeof(_Buffer) - _BufferLen;
    if (available > 0)
    {
        uint16_t read_len = BT401_Read(&_Buffer[_BufferLen], available);
        if (read_len > 0)
        {
            _BufferLen += read_len;
            _tick = HAL_GetTick();   // 更新超时计时
        }
    }

    // 处理超时或缓冲区满的情况
    bool timeout = (_BufferLen > 0) && (HAL_GetTick() - _tick >= TIMEOUT_MS);
    if (_BufferLen >= sizeof(_Buffer) || timeout)
    {
        bool success = _decode();

        if (success)
        {
            beep_start(5, 2);   // 解析成功提示
            _BufferLen = 0;     // 成功解析后清空缓冲区
        } else
        {
            // 解析失败：查找下一个可能的头部，保留后续数据（避免丢失完整帧）
            uint16_t next_header = 0;
            for (next_header = 1; next_header < _BufferLen; next_header++)
            {
                if (_Buffer[next_header] == PROTOCOL_HEADER)
                {
                    // 移动数据到缓冲区头部
                    memmove(_Buffer, &_Buffer[next_header], _BufferLen - next_header);
                    _BufferLen -= next_header;
                    _tick = HAL_GetTick();   // 重置超时计时
                    return;
                }
            }
            // 未找到头部，清空缓冲区
            memset(_Buffer, 0, sizeof(_Buffer));
            BT401_Write(_Buffer, _BufferLen);   // 回显错误数据
            _BufferLen = 0;
        }
    }
}

// 上传参考寄存器值（主动上报）
void upload_reg_value(void)
{
    uint8_t resp_data[1 + 12];   // [数据字节数(1)][6个寄存器×2字节(12)]
    resp_data[0] = 12;           // 数据字节数

    // 填充6个参考寄存器值
    for (int i = 0; i < 6; i++)
    {
        RegisterID reg_id = (RegisterID)(REFRENCE_REG + i);
        _from_uint16(register_get_value(reg_id), &resp_data[1 + 2 * i]);
    }

    // 发送上报帧（命令=读命令，模拟读响应）
    _send_cmd(CMD_READ_REGISTER, resp_data, sizeof(resp_data));
}
