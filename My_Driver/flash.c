#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"
#include "flash.h"

FlashStatus flash_write(uint32_t addr, const uint8_t *buffer, uint32_t bufferLen)
{
    // 检查地址对齐
    if ((addr % 2) != 0)
    {
        return FLASH_ADDR_ALIGN_ERR;
    }

    // 计算擦除页
    uint32_t start_page_addr = addr & FLASH_ERASE_ADDR_MASK;
    uint32_t end_addr = addr + bufferLen;
    uint32_t end_page_addr = (end_addr - 1) & FLASH_ERASE_ADDR_MASK;
    uint32_t pages_to_erase = ((end_page_addr - start_page_addr) / FLASH_ERASE_SIZE) + 1;

    // 擦除配置
    FLASH_EraseInitTypeDef EraseInitStruct = {
        .TypeErase = FLASH_TYPEERASE_PAGES,
        .PageAddress = start_page_addr,
        .NbPages = pages_to_erase};
    uint32_t PageError;

    // 解锁并擦除
    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR);
    if (HAL_FLASHEx_Erase(&EraseInitStruct, &PageError) != HAL_OK)
    {
        HAL_FLASH_Lock();
        return FLASH_ERASE_ERR;
    }

    // 写入数据
    uint32_t cnt = 0;
    while (cnt < bufferLen)
    {
        uint16_t data;
        if (cnt + 1 < bufferLen)
        {
            data = *((uint16_t *)(buffer + cnt));
        }
        else
        {
            data = 0xFF00 | buffer[cnt]; // 显式补0xFF
        }

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr + cnt, data) != HAL_OK)
        {
            HAL_FLASH_Lock();
            return FLASH_WRITE_ERR;
        }
        cnt += 2;

        // 防止极端情况下越界
        if (cnt >= bufferLen)
            break;
    }

    HAL_FLASH_Lock();
    return FLASH_OK;
}
void flash_read(uint32_t addr, uint8_t *buffer, uint32_t bufferLen)
{
    memcpy(buffer, (const void *)addr, bufferLen);
}
