#ifndef __LED_H
#define __LED_H

#ifdef __cplusplus
extern "C" {
#endif

#include "gpio.h"
#include <stdbool.h>
#include <stdint.h>

/* 单色和彩色LED索引枚举 */
typedef enum
{
    LED_MUSIC,
    LED_BT,
    LED_10MIN,
    LED_30MIN,
    LED_60MIN,
    LED_RF,
    LED_G,
    LED_R,
    LED_B,
    LED_COUNT
} LED_Index;

/* LED 工作模式 */
typedef enum
{
    LED_MODE_OFF,    // 关闭
    LED_MODE_ON,     // 常亮
    LED_MODE_BLINK   // 闪烁
} led_mode_t;

/**
 * @brief 初始化所有 LED 控制器，需在系统启动时调用
 */
void led_init(void);

/**
 * @brief 设置指定 LED 的工作模式
 * @param idx LED 索引（LED_Index）
 * @param mode 工作模式（LED_MODE_OFF/ON/BLINK）
 * @param interval_ms 闪烁间隔（毫秒），仅 BLINK 时有效
 */
void led_set_mode(LED_Index idx, led_mode_t mode, uint32_t interval_ms);

/**
 * @brief 周期性调用，更新 BLINK 模式下 LED 的状态（需在主循环中调用）
 */
void led_update_states(void);

/**
 * @brief 读取指定 LED 的当前状态
 * @param idx LED 索引
 * @return true: 点亮，false: 熄灭
 */
bool led_get(LED_Index idx);

/**
 * @brief 时间指示灯：10/30/60 分钟互斥显示，0xFF 全亮
 * @param minutes 10/30/60 或 0xFF
 */
void led_time_select(uint16_t minutes);

#ifdef __cplusplus
}
#endif

#endif /* __LED_H */
