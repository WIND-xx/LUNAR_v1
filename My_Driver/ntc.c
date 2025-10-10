#include "ntc.h"
#include "adc.h"
#include "bt401.h"
#include "led.h"
#include "mytime.h"
#include "pid.h"
#include "register_interface.h"
#include "tim.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* 宏定义 */
#define SAMPLES 10   // ADC采样点数
#define CLAMP(x, low, high) (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))
#define INVALID_TEMP -128.0f   // 无效温度标识

// 系统状态
static float current_temperature = INVALID_TEMP;
static float target_temperature  = 35.0f;
volatile bool adc_complete       = false;   // ADC采样完成标志
uint16_t adc_buffer[SAMPLES];               // ADC采样缓冲区

// 定义PID控制器
PID_Controller heater_pid;
static float last_valid_temp = INVALID_TEMP;   // 上一次有效温度

// ADC转换完成回调函数
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance == hadc1.Instance)
    {
        adc_complete = true;
    }
}

void Temp_init(void)
{
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_buffer, SAMPLES);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);
    PID_Init(&heater_pid, 10.0f, 0.1f, 4.5f, 50.0f, 0.0f, 100.0f);
    // PID_Init(&heater_pid, 6.0f, 0.0f, 0.0f, 100.0f, 0.0f, 100.0f);
    last_valid_temp = INVALID_TEMP;
}

// 比较函数用于 qsort
static int compare_uint16(const void* a, const void* b)
{
    uint16_t va = *(const uint16_t*)a;
    uint16_t vb = *(const uint16_t*)b;
    return (va > vb) ? 1 : (va < vb) ? -1 : 0;
}

static float NTC_temperature(uint16_t adc_value)
{
    float B     = 3950.0f;
    float R2    = 10000.0f;
    float T2    = 25.0f;
    float ntc_R = (adc_value / (4095.0f - adc_value)) * 10000.0f;

    if (ntc_R == 0) return 0.0f;   // 防止除零错误

    float T1 = 1.0f / ((log(ntc_R / R2) / B) + (1.0f / (T2 + 273.15f))) - 273.15f;
    if (T1 < -20.0f || T1 > 100.0f)
    {
        return INVALID_TEMP;   // 温度超出范围
    }
    return T1;
}

// 获取滤波后温度（去掉滑动平均，仅中位数滤波）
float Get_Filtered_Temperature(void)
{
    if (!adc_complete) return last_valid_temp;   // 返回上一次有效温度，避免频繁INVALID_TEMP

    // 1. 中位数滤波
    uint16_t sort_buffer[SAMPLES];
    memcpy(sort_buffer, adc_buffer, sizeof(adc_buffer));
    qsort(sort_buffer, SAMPLES, sizeof(uint16_t), compare_uint16);
    uint16_t median_value = sort_buffer[SAMPLES / 2];
    adc_complete          = false;

    // 检查NTC传感器损坏
    if (median_value < 267 || median_value > 3740)
    {
        return INVALID_TEMP;
    }

    // 2. 计算当前温度
    float temp = NTC_temperature(median_value);

    // 3. 有效性检查
    if (temp == INVALID_TEMP) return last_valid_temp;

    // 4. 更新上一次有效温度
    if (temp >= -20.0f && temp <= 100.0f) last_valid_temp = temp;

    return last_valid_temp;
}

// 超时检测
static bool tick_timeout(uint32_t start, uint32_t timeout)
{
    return (HAL_GetTick() - start) >= timeout;
}
// 过热保护系统（简化版）
static bool overheat_protected  = false;
static uint8_t overheat_counter = 0;

void overheat_protection(float current_temp)
{
    static uint32_t last_check_tick = 0;
    const float OVERHEAT_THRESHOLD  = 65.0f;
    const float HYSTERESIS_TEMP     = 5.0f;

    if (!tick_timeout(last_check_tick, 5000)) return;
    last_check_tick = HAL_GetTick();

    if (overheat_protected)
    {
        if (current_temp < (OVERHEAT_THRESHOLD - HYSTERESIS_TEMP))
        {
            overheat_protected = false;
            overheat_counter   = 0;
            led_set_mode(LED_RF, LED_MODE_ON, 0);
        }
        return;
    }

    if (current_temp >= OVERHEAT_THRESHOLD)
    {
        if (++overheat_counter > 3)
        {
            overheat_protected = true;
            register_set_value(REG_HEATING_STATUS, 0);
            led_set_mode(LED_RF, LED_MODE_BLINK, 200);
        }
    } else if (current_temp < (OVERHEAT_THRESHOLD - HYSTERESIS_TEMP))
    {
        overheat_counter = 0;
        led_set_mode(LED_RF, LED_MODE_ON, 0);
    }
}


// 核心温控循环
void NTC_control(uint16_t dt_ms)
{
    static uint32_t last_control = 0;
    uint32_t now                 = HAL_GetTick();

    if (now - last_control < dt_ms) return;
    last_control = now;

    if (!register_get_value(REG_HEATING_STATUS) || overheat_protected)
    {
        __HAL_TIM_SetCompare(&htim1, TIM_CHANNEL_4, 0);
        PID_Reset(&heater_pid);   // 重置PID控制器
        return;
    }

    float temp = Get_Filtered_Temperature() + 2.0f;   // 获取当前温度并加2度补偿
    // DEBUG_PRINTF("Temp%.2f\n", temp);
    current_temperature = temp;

    if (temp == INVALID_TEMP)
    {
        __HAL_TIM_SetCompare(&htim1, TIM_CHANNEL_4, 0);   // 无效温度时关闭加热
        PID_Reset(&heater_pid);                           // 重置PID控制器
        return;
    }

    overheat_protection(temp);

    uint16_t pid_out = PID(&heater_pid, temp, target_temperature, dt_ms);
    // DEBUG_PRINTF("PIDOutput: %d\n", pid_out);
    __HAL_TIM_SetCompare(&htim1, TIM_CHANNEL_4, pid_out);
}

// 公共接口函数
void set_target_temperature(uint8_t temp)
{
    target_temperature = temp;
}

uint8_t get_target_temperature(void)
{
    return target_temperature;
}

int8_t get_current_temperature(void)
{
    return current_temperature;
}

bool is_overheat(void)
{
    return overheat_protected;
}

void clear_overheat_protection(void)
{
    overheat_protected = false;
    overheat_counter   = 0;
    register_set_value(REG_HEATING_STATUS, 0);
    led_set_mode(LED_RF, LED_MODE_OFF, 0);
}
