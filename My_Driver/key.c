#include "key.h"
#include "alarm.h"
#include "beep.h"
#include "bt401.h"
#include "hardware_register.h"
#include "led.h"
#include "main.h"
#include "ntc.h"
#include "register_interface.h"
#include "shortcut.h"
#include <stdio.h>
#include <string.h>

#define KEY_NONE 0
#define KEY_SHORT_PRESS 1
#define KEY_LONG_PRESS 2
#define KEY_DOUBLE_CLICK 3

#define LONG_PRESS_THRESHOLD 100    // 100 * 20ms = 2秒
#define DOUBLE_CLICK_THRESHOLD 50   // 50 * 20ms = 500毫秒

// 全局变量

bool ble_key_pressed             = false;
static unsigned char key_state   = KEY_NONE;
static unsigned char key_counter = 0;
// unsigned char key_last = 0;
// unsigned char key_last_time = 0;

static void key_event_handler(unsigned char key, unsigned char event);
static void key_short_event(unsigned char key);
static void key_long_event(unsigned char key);

unsigned char get_key()
{
    if (HAL_GPIO_ReadPin(POWER_DC_GPIO_Port, POWER_DC_Pin) == 0)
    {
        return MODE_POWER;
    }

    uint8_t Mode                     = 0;
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    // 定义每一行的配置信息
    typedef struct
    {
        uint16_t outputPin;      // 输出引脚
        uint16_t inputPins[5];   // 输入引脚列表
        uint8_t modeValues[5];   // 对应的按键模式值
    } KeyRowConfig;

    // 配置每一行的键盘设置
    KeyRowConfig rows[] = {{GPIO_PIN_9, {GPIO_PIN_8, GPIO_PIN_3, GPIO_PIN_5, GPIO_PIN_4, GPIO_PIN_15}, {1, 2, 3, 4, 5}},
                           {GPIO_PIN_8, {GPIO_PIN_3, GPIO_PIN_5, GPIO_PIN_4, GPIO_PIN_15, 0}, {6, 7, 8, 9, 0}},
                           {GPIO_PIN_3, {GPIO_PIN_5, GPIO_PIN_4, GPIO_PIN_15, 0, 0}, {12, 10, 11, 0, 0}},
                           {GPIO_PIN_5, {GPIO_PIN_4, GPIO_PIN_15, 0, 0, 0}, {14, 13, 0, 0, 0}},
                           {GPIO_PIN_4, {GPIO_PIN_15, 0, 0, 0, 0}, {15, 0, 0, 0, 0}}};

    // 遍历每一行的配置
    for (int i = 0; i < sizeof(rows) / sizeof(rows[0]); i++)
    {
        // 配置输出引脚
        GPIO_InitStruct.Pin   = rows[i].outputPin;
        GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
        HAL_GPIO_WritePin(GPIOB, rows[i].outputPin, GPIO_PIN_RESET);

        // 配置输入引脚并检测按键
        for (int j = 0; j < 5; j++)
        {
            if (rows[i].inputPins[j] != 0)
            {
                GPIO_InitStruct.Pin  = rows[i].inputPins[j];
                GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
                GPIO_InitStruct.Pull = GPIO_PULLUP;
                HAL_GPIO_Init((rows[i].inputPins[j] == GPIO_PIN_15) ? GPIOA : GPIOB, &GPIO_InitStruct);

                // 检测按键是否按下
                if (HAL_GPIO_ReadPin((rows[i].inputPins[j] == GPIO_PIN_15) ? GPIOA : GPIOB, rows[i].inputPins[j]) ==
                    GPIO_PIN_RESET)
                {
                    Mode = rows[i].modeValues[j];
                }
            }
        }

        // 释放当前行的输出引脚
        HAL_GPIO_WritePin(GPIOB, rows[i].outputPin, GPIO_PIN_SET);
    }

    return Mode;
}

void key_scan()
{
    static unsigned char before = 0;
    unsigned char now           = get_key();

    if (before == 0 && now != 0)
    {
        key_counter = 0;
        key_state   = KEY_NONE;
    } else if (before != 0 && now == 0)
    {
        if (key_state == KEY_NONE)
        {
            if (key_counter < LONG_PRESS_THRESHOLD)
            {
                key_event_handler(before, KEY_SHORT_PRESS);
            }
        }
        key_counter = 0;
    } else if (now != 0)
    {
        key_counter++;
        if (key_counter >= LONG_PRESS_THRESHOLD)
        {
            if (key_state == KEY_NONE)
            {
                key_state = KEY_LONG_PRESS;
                key_event_handler(now, KEY_LONG_PRESS);
            }
        }
    }

    before = now;
}

static void key_event_handler(unsigned char key, unsigned char event)
{

    // 根据按键事件类型处理
    switch (event)
    {
        case KEY_SHORT_PRESS: key_short_event(key); break;
        case KEY_LONG_PRESS: key_long_event(key); break;
        case KEY_DOUBLE_CLICK:
            // 处理双击事件（可扩展）
            break;
        default: break;
    }
}

static void handle_media_control(unsigned char key)
{
    switch (key)
    {
        case MODE_PLAY_PAUSE: AT_PRINTF("AT+CB\r\n"); break;
        case MODE_PREV: AT_PRINTF("AT+CD\r\n"); break;
        case MODE_NEXT: AT_PRINTF("AT+CC\r\n"); break;
        case MODE_VOL_DOWN:
            send_at_command("AT+CF\r\n", AT_TIMEOUT);
            send_at_command("AT+CF\r\n", AT_TIMEOUT);
            break;
        case MODE_VOL_UP:
            send_at_command("AT+CE\r\n", AT_TIMEOUT);
            send_at_command("AT+CE\r\n", AT_TIMEOUT);
            break;
        default: break;
    }
}

static void toggle_heating()
{
    uint8_t heating_status = register_get_value(REG_HEATING_STATUS);
    register_set_value(REG_HEATING_STATUS, !heating_status);
}

static void adjust_heating_level(int delta)
{
    int current_level = register_get_value(REG_HEATING_LEVEL);
    int new_level     = current_level + delta;

    // Clamp the new level between 0 and 2
    new_level = (new_level < 0) ? 0 : (new_level > 2) ? 2 : new_level;

    if (new_level != current_level)
    {
        register_set_value(REG_HEATING_LEVEL, new_level);
    } else
    {
        // Beep only if the level is already at the boundary
        beep_start(20, 10);   // 短暂蜂鸣提示
    }
}

static void set_heating_timer(unsigned char key)
{
    uint8_t timer = 0;
    // 按下定时，再按取消定时
    switch (key)
    {
        case MODE_MIN60: timer = led_get(LED_60MIN) ? 0 : 60; break;
        case MODE_MIN30: timer = led_get(LED_30MIN) ? 0 : 30; break;
        case MODE_MIN10: timer = led_get(LED_10MIN) ? 0 : 10; break;
        default: timer = 0; break;
    }
    register_set_value(REG_HEATING_TIMER, timer);
}

void shutdown()
{
    // save_config();   // 保存配置
    save_alarms();   // 保存闹钟
    HAL_PWR_EnterSTANDBYMode();
    HAL_GPIO_WritePin(POWER_GPIO_Port, POWER_Pin, GPIO_PIN_RESET);
}

uint8_t mode_control(BT_MODE mode)
{
    switch (mode)
    {
        case NONE_MODE:
            // 释放掉所有资源，进入空闲模式
            send_at_command("AT+BA01\r\n", AT_TIMEOUT);
            send_at_command("AT+BA07\r\n", AT_TIMEOUT);
            send_at_command("AT+CM08\r\n", AT_TIMEOUT);
            ble_key_pressed = false;
            led_set_mode(LED_MUSIC, LED_MODE_OFF, 0);
            led_set_mode(LED_BT, LED_MODE_OFF, 0);
            set_music_active(false);
            return 1;   // 成功切换到空闲模式
        case MUSIC_MODE:
            // 切换到助眠音乐模式
            send_at_command("AT+CM04\r\n", AT_TIMEOUT);
            set_music_active(true);
            led_set_mode(LED_MUSIC, LED_MODE_ON, 0);
            send_at_command("AT+AC01\r\n", AT_TIMEOUT);   // 循环
            send_at_command("AT+AA01\r\n", AT_TIMEOUT);   // 播放
            ble_key_pressed = false;
            return 2;   // 成功切换到助眠音乐模式
        case BLUETOOTH_MODE:
            // 切换到蓝牙音乐模式
            send_at_command("AT+CM01\r\n", AT_TIMEOUT);
            led_set_mode(LED_MUSIC, LED_MODE_OFF, 0);
            set_music_active(false);
            send_at_command("AT+BA06\r\n", AT_TIMEOUT);   // 打开蓝牙音频可发现
            // send_at_command("AT+BA08\r\n", AT_TIMEOUT);   // 播放
            send_at_command("AT+CB\r\n", AT_TIMEOUT);
            ble_key_pressed = true;
            return 3;   // 成功切换到蓝牙音乐模式
    }
    return 0;   // 无效模式
}

static void key_short_event(unsigned char key)
{
    if (ring_flag == 1)
    {
        ring_flag = 0;
        send_at_command("AT+CA10\r\n", 50);
        stop_music_task();
        return;
    }
    if (is_overheat())
    {
        // 如果处于过热保护状态
        clear_overheat_protection();
        return;
    }
    switch (key)
    {
        case MODE_POWER: shutdown(); break;
        case MODE_ZHUMIAN: mode_control(MUSIC_MODE); break;
        case MODE_BLUETOOTH: mode_control(BLUETOOTH_MODE); break;
        case MODE_PLAY_PAUSE:
        case MODE_PREV:
        case MODE_NEXT:
        case MODE_VOL_DOWN:
        case MODE_VOL_UP: handle_media_control(key); break;
        case MODE_HEAT: toggle_heating(); break;
        case MODE_HEAT_MINUS: adjust_heating_level(1); break;
        case MODE_HEAT_PLUS: adjust_heating_level(-1); break;
        case MODE_MIN60:
        case MODE_MIN30:
        case MODE_MIN10: set_heating_timer(key); break;
        case MODE_SHORTCUT_1:
        case MODE_SHORTCUT_2: execute_shortcut_keys(key - MODE_SHORTCUT_1 + 1); break;
        default: break;
    }
}

static void key_long_event(unsigned char key)
{
    beep_start(50, 10);

    switch (key)
    {
        case MODE_MIN60:
        case MODE_MIN30:
        case MODE_MIN10: set_heating_timer(0); break;
        case MODE_SHORTCUT_1:
        case MODE_SHORTCUT_2: save_shortcut(key - MODE_SHORTCUT_1 + 1); break;
        case MODE_BLUETOOTH: mode_control(NONE_MODE); break;
        default: break;
    }
}
