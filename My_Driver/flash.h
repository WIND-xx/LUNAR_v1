/**
 * @file flash.h
 * @author ZhangYu
 * @date 2023-12-03
 * @brief
 */

#ifndef FLASH_INCLUDED_1143530300533791
#define FLASH_INCLUDED_1143530300533791

#include <stdint.h>

#define FLASH_START_ADDR      (0x08000000 + 50 * 1024)      // 48KB起始地址
#define FLASH_ALARM_ADDR      (FLASH_START_ADDR + 1 * 1024) // 确保不覆盖代码区
#define FLASH_ERASE_SIZE      (1024)                        // STM32F103页大小为1KB
#define FLASH_ERASE_ADDR_MASK (~(FLASH_ERASE_SIZE - 1))

// 错误码定义
typedef enum {
    FLASH_OK = 0,
    FLASH_ERASE_ERR,
    FLASH_WRITE_ERR,
    FLASH_ADDR_ALIGN_ERR
} FlashStatus;

FlashStatus flash_write(uint32_t addr, const uint8_t *buffer, uint32_t bufferLen);
void flash_read(uint32_t addr, uint8_t *buffer, uint32_t bufferLen);

#endif /* FLASH_INCLUDED_1143530300533791 */
