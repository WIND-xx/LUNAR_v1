#pragma once

#include <stdint.h>
void beep_pwm_update(void);
void beep_start(uint32_t duration_ms, uint8_t duty_cycle);
void beep_update(void);
