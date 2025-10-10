#ifndef __NTC_H
#define __NTC_H

#include "stm32f1xx_hal.h"
#include <stdbool.h>

void Temp_init(void);
void NTC_control(uint16_t dt);

void    set_target_temperature(uint8_t temp);
uint8_t get_target_temperature(void);

bool is_overheat(void);
void clear_overheat_protection(void);

#endif   // __NTC_H
