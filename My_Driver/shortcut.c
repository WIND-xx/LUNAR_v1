#include "shortcut.h"
#include "bt401.h"
#include "hardware_register.h"
#include "key.h"
#include "led.h"
#include "register_interface.h"
#include "stdint.h"

#define SHORTCUT_COUNT 2   // 定义快捷键数量

static uint16_t _compose_shortcut_data(Shortcut_struct* shortcut);

// 存储多个快捷操作的数组
Shortcut_struct shortcut[SHORTCUT_COUNT] = {0};

// 新增结构体保存系统状态
typedef struct
{
    uint8_t heat_level;
    uint8_t music_id;
    uint16_t timer_minutes;
    uint8_t heating_status;
    bool music_playing;
} SystemState;

// 新增全局变量
static SystemState original_state;       // 保存原始状态
static uint8_t active_shortcut_id = 0;   // 当前激活的快捷键ID（0表示无激活）
int8_t reset_act_shortcut_id(int8_t id)
{
    active_shortcut_id = 0;   // 重置激活状态
    return 0;                 // 成功
}
void shortcut_init(void)
{
    shortcut[0].heat_level    = 0;    // 35℃，默认档位
    shortcut[0].music_id      = 0;    // 默认音乐ID
    shortcut[0].timer_minutes = 10;   // 默认定时器时间

    shortcut[1].heat_level    = 2;    // 55℃，默认档位
    shortcut[1].music_id      = 2;    // 默认音乐ID
    shortcut[1].timer_minutes = 30;   // 默认定时器时间

    uint16_t reg_value1 = _compose_shortcut_data(&shortcut[0]);
    uint16_t reg_value2 = _compose_shortcut_data(&shortcut[1]);
    register_set_value(REG_SHORTCUT_KEY1, reg_value1);   // 保存快捷键1到寄存器
    register_set_value(REG_SHORTCUT_KEY2, reg_value2);   // 保存快捷键2到寄存器
}
static uint16_t _compose_shortcut_data(Shortcut_struct* shortcut)
{
    uint16_t reg_value = 0;
    // 热敷档位（位0-1）: 左移0位，掩码0x03（确保值不超过3）
    reg_value |= (shortcut->heat_level & 0x03) << 0;

    // 音乐ID（位2-7）: 左移2位，掩码0x3F（确保值不超过63）
    reg_value |= (shortcut->music_id & 0x3F) << 2;

    // 定时时间（位8-15）: 左移8位，掩码0xFF（确保值不超过255）
    reg_value |= (shortcut->timer_minutes & 0xFF) << 8;

    return reg_value;
}
static void _parse_shortcut_data(uint16_t reg_value, Shortcut_struct* shortcut)
{
    // 热敷档位（位0-1）: 右移0位后取低2位
    shortcut->heat_level = (reg_value >> 0) & 0x03;

    // 音乐ID（位2-7）: 右移2位后取低6位
    shortcut->music_id = (reg_value >> 2) & 0x3F;

    // 定时时间（位8-15）: 右移8位后取低8位
    shortcut->timer_minutes = (reg_value >> 8) & 0xFF;
}
// 辅助函数：保存单个快捷键
static void _save_single_shortcut(uint8_t index)
{
    shortcut[index].heat_level    = register_get_value(REG_HEATING_LEVEL);   // 获取当前热敷档位
    shortcut[index].music_id      = query_music_id();                        // 获取当前音乐ID
    shortcut[index].timer_minutes = register_get_value(REG_HEATING_TIMER);   // 获取当前定时时间

    uint16_t reg_value = _compose_shortcut_data(&shortcut[index]);   // 组合快捷键数据到寄存器值并保存到寄存器

    register_set_value((RegisterID)(REG_SHORTCUT_KEY1 + index), reg_value);   // 动态计算寄存器地址
}

// 辅助函数：执行单个快捷键
static void _execute_single_shortcut(uint8_t index)
{
    uint16_t value = register_get_value((RegisterID)(REG_SHORTCUT_KEY1 + index));   // 动态计算寄存器地址
    _parse_shortcut_data(value, &shortcut[index]);                                  // 解析寄存器值到快捷键结构体
    if (led_get(LED_MUSIC) == false)                                                // 如果音乐LED灯是关闭状态
    {
        mode_control(MUSIC_MODE);
    }
    HAL_Delay(10);
    AT_PRINTF("AT+AB/%d\r\n", shortcut[index].music_id);                    // 播放指定序号音乐
    register_set_value(REG_HEATING_LEVEL, shortcut[index].heat_level);      //  设置热敷档位
    register_set_value(REG_HEATING_TIMER, shortcut[index].timer_minutes);   //  设置定时时间
    register_set_value(REG_HEATING_STATUS, 1);                              //  设置加热状态
}

// 保存快捷键
void save_shortcut(uint8_t shortcut_id)
{
    if (shortcut_id < 1 || shortcut_id > SHORTCUT_COUNT)
    {
        // 错误处理：非法的快捷键 ID
        return;
    }
    _save_single_shortcut(shortcut_id - 1);   // 转换为数组索引
}

// 执行快捷键
void execute_shortcut_keys(uint8_t id)
{
    if (id < 1 || id > SHORTCUT_COUNT)
    {
        // 错误处理：非法的快捷键 ID
        return;
    }
    // 如果按下的是已激活的快捷键：撤销操作
    if (active_shortcut_id == id)
    {
        // 恢复加热状态
        register_set_value(REG_HEATING_LEVEL, original_state.heat_level);
        register_set_value(REG_HEATING_TIMER, original_state.timer_minutes);
        register_set_value(REG_HEATING_STATUS, original_state.heating_status);

        // 恢复音乐状态
        if (original_state.music_playing)
        {
            if (!led_get(LED_MUSIC))
            {
                mode_control(MUSIC_MODE);   // 重新打开音乐模式
            }
            AT_PRINTF("AT+AB/%d\r\n", original_state.music_id);   // 播放原始音乐
        } else
        {
            if (led_get(LED_MUSIC))
            {
                mode_control(NONE_MODE);   // 关闭音乐模式
            }
            AT_PRINTF("AT+AA\r\n");   // 停止音乐播放
        }

        active_shortcut_id = 0;   // 重置激活状态
        return;
    }

    // 保存当前系统状态（用于可能的撤销）
    original_state.heat_level     = register_get_value(REG_HEATING_LEVEL);
    original_state.timer_minutes  = register_get_value(REG_HEATING_TIMER);
    original_state.heating_status = register_get_value(REG_HEATING_STATUS);
    original_state.music_playing  = led_get(LED_MUSIC);
    original_state.music_id       = query_music_id();
    active_shortcut_id            = id;   // 更新激活的快捷键ID

    _execute_single_shortcut(id - 1);   // 转换为数组索引
}
