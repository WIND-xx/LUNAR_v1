#ifndef __BT401_H
#define __BT401_H

#include "stm32f1xx_hal.h"
#include <stdarg.h>

#define BT401_BUFFER_SIZE 128

// typedef enum {
//     BT401_OK    = 0x00U,
//     BT401_BUSY  = 0x01U,
//     BT401_ERROR = 0x02U
// } BT401_Status;

void HAL_UART_TxCpltCallback(UART_HandleTypeDef* huart);
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef* huart, uint16_t Size);

void     BT401_Init(void);
uint16_t BT401_Read(uint8_t* buffer, uint16_t size);
uint16_t BT401_Write(uint8_t* buffer, uint16_t size);
uint16_t BT401_Printf(const char* format, ...);

#define AT_PRINTF BT401_Printf
#define DEBUG_PRINTF BT401_Printf
#define AT_TIMEOUT 50   // AT指令超时时间
#define CMD_TIMEOUT_MS 50
#endif
