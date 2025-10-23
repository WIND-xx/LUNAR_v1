// register_interface.c
#include "register_interface.h"
#include "alarm.h"
#include "beep.h"
#include "bt401.h"
#include "crc16.h"
#include "flash.h"
#include "hardware_register.h"
#include "led.h"
#include "protocol.h"
#include "shortcut.h"
#include <stddef.h>
#include <string.h>

static uint16_t _RegValue[REG_COUNT - REFRENCE_REG + 1];
static uint16_t _RegValueBackup[REG_COUNT - REFRENCE_REG + 1];


// static inline void _load_config(void)
// {
//     static const uint16_t _defaultValue[] = {
//         0,   // RF工作状态
//         0,   // RF档位
//         0,   // 工作定时(分钟)
//         0,   // 快捷键1
//         0,   // 快捷键2
//     };

//     // 读取配置
//     flash_read(FLASH_START_ADDR, (uint8_t*)_RegValue, sizeof(_RegValue));
//     if (_calc_check_value((uint8_t*)_RegValue, sizeof(_RegValue) - sizeof(uint16_t)) !=
//         _RegValue[ARRAY_SIZE(_RegValue) - 1])
//     {
//         memcpy(_RegValue, _defaultValue, sizeof(_defaultValue));
//     }

//     // 初始化寄存器值
//     _RegValue[REG_HEATING_TIMER - REFRENCE_REG]  = 0;
//     _RegValue[REG_HEATING_LEVEL - REFRENCE_REG]  = 0;
//     _RegValue[REG_HEATING_STATUS - REFRENCE_REG] = 0;

//     // 备份寄存器值
//     memcpy(_RegValueBackup, _RegValue, sizeof(_RegValue));
// }

void save_config(void)
{
    if (memcmp(_RegValueBackup, _RegValue, sizeof(_RegValue)) != 0)
    {
        _RegValue[ARRAY_SIZE(_RegValue) - 1] =
            _calc_check_value((uint8_t*)_RegValue, sizeof(_RegValue) - sizeof(uint16_t));
        if (flash_write(FLASH_START_ADDR, (uint8_t*)_RegValue, sizeof(_RegValue)) == FLASH_OK)
        {
            memcpy(_RegValueBackup, _RegValue, sizeof(_RegValue));
        } else
        {
            DEBUG_PRINTF("Flash write error\r\n");
        }
    }
}

void register_interface_init(void)
{
    // _load_config();
    update_hardware_registers(&_RegValue[1], 3);
}

bool register_set_value(RegisterID id, uint16_t value)
{
    if (id < REFRENCE_REG || id >= REG_COUNT)
    {
        return false;
    }

    uint16_t* reg_ptr = &_RegValue[id - REFRENCE_REG];
    if (*reg_ptr == value)
    {
        return false;   // 寄存器值未改变
    }

    *reg_ptr = value;
    _do_reg_changed(id, value);
    return true;
}

uint16_t register_get_value(RegisterID id)
{
    return (id >= REFRENCE_REG && id < REG_COUNT) ? _RegValue[id - REFRENCE_REG] : 0;
}

void _do_reg_changed(uint16_t reg, uint16_t value)
{
    switch (reg)
    {
        case REG_DELETE_ALARM: delete_alarm(value); break;
        case REG_POWER_SWITCH: shutdown(); break;
        case REG_EXECUTE_SHORTCUT: execute_shortcut_keys((uint8_t)value); break;
        case REG_HEATING_STATUS: rf_switch(value); break;
        case REG_HEATING_LEVEL: rf_level(value); break;
        case REG_HEATING_TIMER: rf_time(value); break;
        default: break;
    }
}
