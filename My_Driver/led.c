#include "led.h"
#include "gpio.h"
#include "main.h"
#include <stdbool.h>

/* LED GPIO 配置 */
typedef struct
{
    GPIO_TypeDef* port;
    uint16_t pin;
    GPIO_PinState active_high;   // 有效点亮电平
} led_cfg_t;

/* 单色 LED 配置表（前 6 项） */
static const led_cfg_t led_cfg[LED_RF + 1] = {
    [LED_MUSIC] = {LED4_GPIO_Port, LED4_Pin, GPIO_PIN_RESET},
    [LED_BT]    = {LED3_GPIO_Port, LED3_Pin, GPIO_PIN_RESET},
    [LED_10MIN] = {LED6_GPIO_Port, LED6_Pin, GPIO_PIN_RESET},
    [LED_30MIN] = {LED5_GPIO_Port, LED5_Pin, GPIO_PIN_RESET},
    [LED_60MIN] = {LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET},
    [LED_RF]    = {LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET},
};

/* 彩色 LED 三通道统一管理 */
static const struct
{
    GPIO_TypeDef* port;
    uint16_t pin;
} color_pin[3] = {
    [0] = {LED_G_GPIO_Port, LED_G_Pin},
    [1] = {LED_R_GPIO_Port, LED_R_Pin},
    [2] = {LED_B_GPIO_Port, LED_B_Pin},
};

/* 每个 LED 的控制状态 */
typedef struct
{
    led_mode_t mode;
    uint32_t interval;
    uint32_t next_toggle;
} led_ctrl_t;

static led_ctrl_t led_ctrl[LED_COUNT];

/* 低层：直接改写 GPIO */
static void led_hw_write(LED_Index idx, bool on)
{
    if (idx <= LED_RF)
    {
        /* 单色 LED */
        const led_cfg_t* c = &led_cfg[idx];
        HAL_GPIO_WritePin(
            c->port, c->pin, on ? c->active_high : (c->active_high == GPIO_PIN_SET ? GPIO_PIN_RESET : GPIO_PIN_SET));
    } else
    {
        /* 先全关，再打开指定通道 */
        for (int ch = 0; ch < 3; ch++)
        {
            HAL_GPIO_WritePin(color_pin[ch].port, color_pin[ch].pin, GPIO_PIN_SET);
        }
        if (on)
        {
            int ch = (idx == LED_G ? 0 : idx == LED_R ? 1 : 2);
            HAL_GPIO_WritePin(color_pin[ch].port, color_pin[ch].pin, GPIO_PIN_RESET);
        }
    }
}

/* 低层：读取当前 GPIO 状态 */
static bool led_hw_read(LED_Index idx)
{
    if (idx <= LED_RF)
    {
        const led_cfg_t* c = &led_cfg[idx];
        return HAL_GPIO_ReadPin(c->port, c->pin) == c->active_high;
    } else
    {
        int ch = (idx == LED_G ? 0 : idx == LED_R ? 1 : 2);
        return HAL_GPIO_ReadPin(color_pin[ch].port, color_pin[ch].pin) == GPIO_PIN_RESET;
    }
}

/* 初始化：所有 LED OFF，默认闪烁间隔 500ms */
void led_init(void)
{
    for (int i = 0; i < LED_COUNT; i++)
    {
        led_ctrl[i].mode        = LED_MODE_OFF;
        led_ctrl[i].interval    = 500;
        led_ctrl[i].next_toggle = 0;
        led_hw_write((LED_Index)i, false);
    }
    led_set_mode(LED_B, LED_MODE_ON, 0);
}

/* 设置单个 LED 模式（OFF/ON/BLINK）*/
void led_set_mode(LED_Index idx, led_mode_t mode, uint32_t interval_ms)
{
    if (idx >= LED_COUNT) return;
    led_ctrl[idx].mode = mode;
    if (mode == LED_MODE_BLINK)
    {
        led_ctrl[idx].interval    = interval_ms;
        led_ctrl[idx].next_toggle = HAL_GetTick() + interval_ms;
        led_hw_write(idx, true);
    } else
    {
        led_hw_write(idx, mode == LED_MODE_ON);
    }
}

/* 在主循环中周期调用，更新所有 BLINK 模式的 LED */
void led_update_states(void)
{
    uint32_t now = HAL_GetTick();
    for (int i = 0; i < LED_COUNT; i++)
    {
        if (led_ctrl[i].mode == LED_MODE_BLINK && (int32_t)(now - led_ctrl[i].next_toggle) >= 0)
        {
            bool cur = led_hw_read((LED_Index)i);
            led_hw_write((LED_Index)i, !cur);
            led_ctrl[(LED_Index)i].next_toggle = now + led_ctrl[i].interval;
        }
    }
}

/* 读取当前 LED 状态 */
bool led_get(LED_Index idx)
{
    if (idx >= LED_COUNT) return false;
    return led_hw_read(idx);
}

/* 专用：时间选择指示（三灯互斥/全亮/全灭） */
void led_time_select(uint16_t minutes)
{
    // 先全部关闭
    led_hw_write(LED_10MIN, false);
    led_hw_write(LED_30MIN, false);
    led_hw_write(LED_60MIN, false);

    if (minutes > 30)
    {
        led_hw_write(LED_60MIN, true);
    } else if (minutes > 10)
    {
        led_hw_write(LED_30MIN, true);
    } else if (minutes > 0)
    {
        led_hw_write(LED_10MIN, true);
    } else
    {
        // 全部关闭
        led_hw_write(LED_10MIN, false);
        led_hw_write(LED_30MIN, false);
        led_hw_write(LED_60MIN, false);
    }
}
