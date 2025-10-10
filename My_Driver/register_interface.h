#ifndef REGISTER_INTERFACE_H
#    define REGISTER_INTERFACE_H

#    include <stdbool.h>
#    include <stdint.h>
typedef enum
{
    REG_POWER_SWITCH = 0,     // 关机
    REG_UTC_TIMESTAMP_HIGH,   // UTC时间戳（高位）
    REG_UTC_TIMESTAMP_LOW,    // UTC时间戳（低位）
    REG_ALARM_SET_HIGH,       // 闹钟（高位）
    REG_ALARM_SET_LOW,        // 闹钟（低位）
    REG_DELETE_ALARM,         // 删除闹钟
    REG_EXECUTE_SHORTCUT,     // 执行快捷键
    REG_HEATING_STATUS,       // 热敷工作状态
    REG_HEATING_LEVEL,        // 热敷档位
    REG_HEATING_TIMER,        // 热敷工作定时
    REG_SHORTCUT_KEY1,        // 快捷键1
    REG_SHORTCUT_KEY2,        // 快捷键2
    REG_COUNT,
} RegisterID;

#    define REFRENCE_REG REG_HEATING_STATUS
#    define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

uint16_t _calc_check_value(const uint8_t data[], uint32_t dataLen);
uint16_t _to_uint16(const uint8_t data[]);
void _from_uint16(uint16_t value, uint8_t data[]);
// 初始化接口
void register_interface_init(void);

// 设置寄存器值
bool register_set_value(RegisterID id, uint16_t value);
void _do_reg_changed(uint16_t reg, uint16_t value);
// 获取寄存器值
uint16_t register_get_value(RegisterID id);

void save_config(void);

#endif
// /* REGISTER_INTERFACE_H */
