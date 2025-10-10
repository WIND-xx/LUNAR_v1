#ifndef __KEY_H
#define __KEY_H

#include "main.h"

#define KEY1_Input HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_8)
#define KEY2_Input HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_3)
#define KEY3_Input HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_5)
#define KEY4_Input HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4)
#define KEY5_Input HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_15)

#define KEY6_Input HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_3)
#define KEY7_Input HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_5)
#define KEY8_Input HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4)
#define KEY9_Input HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_15)

#define KEY10_Input HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4)
#define KEY11_Input HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_15)
#define KEY12_Input HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_5)

#define KEY14_Input HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4)
#define KEY13_Input HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_15)

#define KEY15_Input HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_15)

#define KEY_Power_Input HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_10)

typedef enum
{
    MODE_NONE       = 0,
    MODE_ZHUMIAN    = 1,
    MODE_BLUETOOTH  = 2,
    MODE_PLAY_PAUSE = 3,
    MODE_MIN10      = 4,
    MODE_MIN60      = 5,
    MODE_PREV       = 6,
    MODE_NEXT       = 7,
    MODE_VOL_DOWN   = 8,
    MODE_VOL_UP     = 9,
    MODE_HEAT_PLUS  = 10,
    MODE_HEAT_MINUS = 11,
    MODE_SHORTCUT_1 = 12,
    MODE_SHORTCUT_2 = 13,
    MODE_MIN30      = 14,
    MODE_HEAT       = 15,
    MODE_POWER      = 18
} KeyMode;

typedef enum
{
    NONE_MODE = 0,
    MUSIC_MODE,
    BLUETOOTH_MODE,
} BT_MODE;

extern bool ble_key_pressed;

void    shutdown(void);
void    key_scan(void);
uint8_t mode_control(BT_MODE mode);
#endif
