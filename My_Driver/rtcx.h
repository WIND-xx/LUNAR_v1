#pragma once

#include "stm32f1xx_hal.h"
#include "rtc.h"
#include "time.h"

HAL_StatusTypeDef XX_RTC_SetTime(struct tm *time);
struct tm *XX_RTC_GetTime();
void XX_RTC_Init();
