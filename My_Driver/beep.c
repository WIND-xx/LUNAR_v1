#include "beep.h"
#include "gpio.h"
#include "main.h"

volatile uint16_t pwm_count      = 0;     // 类型修正为 uint16_t
volatile uint16_t pwm_period     = 200;   // 200kHz/200=1kHz
volatile uint16_t pwm_duty_cycle = 0;     // 类型修正为 uint16_t
volatile uint32_t beep_timer     = 0;     // 单位：ms

// 中断调用
void beep_pwm_update(void)
{
    pwm_count++;
    if (pwm_count >= pwm_period)
    {
        pwm_count = 0;
    }
    HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, (pwm_count < pwm_duty_cycle) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void beep_start(uint32_t duration_ms, uint8_t duty_cycle)
{
    if (duration_ms == 0)
    {
        __disable_irq();   // 禁用中断
        pwm_duty_cycle = 0;
        __enable_irq();   // 启用中断
        return;
    }
    // 修正占空比限制逻辑
    uint16_t clamped_duty = (duty_cycle > pwm_period) ? pwm_period : duty_cycle;
    __disable_irq();   // 禁用中断
    pwm_duty_cycle = clamped_duty;
    __enable_irq();   // 启用中断
    beep_timer = duration_ms;
}

void beep_update(void)
{
    if (beep_timer > 0)
    {
        beep_timer--;
        if (beep_timer == 0)
        {
            __disable_irq();   // 禁用中断
            pwm_duty_cycle = 0;
            __enable_irq();   // 启用中断
        }
    }
}
