#include "bt401.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "usart.h"

#define USARTx_HANDLE huart3
#define UART_RX_BUFFER_SIZE 1             // 每次只接收一个字节
#define RING_RX_SIZE 512                  // 缓存区大小
#define BT401_TX_FORMAT_BUFFER_SIZE 128   // 根据需求调整缓冲区大小

/* 私有全局变量 */
#pragma pack(push, 1)
typedef struct
{
    volatile uint16_t head;
    volatile uint16_t tail;
    uint8_t buffer[RING_RX_SIZE];
    volatile uint8_t overflow;   // 缓冲区溢出标志
} RingBuffer;

RingBuffer rx_ring = {0};
uint8_t uart_rx_buffer[UART_RX_BUFFER_SIZE];   // 接收缓冲区
#pragma pack(pop)

/* 初始化函数 */
void UART_Init(void)
{
    // 先执行HAL_UART_MspInit（CubeMX生成）

    // 启动中断接收方式，每次接收一个字节
    HAL_UART_Receive_IT(&USARTx_HANDLE, uart_rx_buffer, 1);
}

/* UART接收完成回调函数 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef* huart)
{
    if (huart == &USARTx_HANDLE)
    {
        // 将接收到的字节放入环形缓冲区
        uint8_t received_byte = uart_rx_buffer[0];

        __disable_irq();

        uint16_t free_space = RING_RX_SIZE - ((rx_ring.head - rx_ring.tail) & (RING_RX_SIZE - 1));
        if (free_space > 0)
        {
            uint16_t write_idx        = rx_ring.head & (RING_RX_SIZE - 1);
            rx_ring.buffer[write_idx] = received_byte;
            rx_ring.head += 1;
            rx_ring.overflow = 0;
        } else
        {
            rx_ring.overflow = 1;   // 缓冲区溢出
        }

        __enable_irq();

        // 继续下一次中断接收
        HAL_UART_Receive_IT(huart, uart_rx_buffer, 1);
    }
}

/* 发送数据函数（阻塞改非阻塞）*/
uint16_t UART_Write(const uint8_t* data, uint16_t len)
{
    if (len == 0 || data == NULL) return 0;

    // 直接调用中断发送
    if (HAL_UART_Transmit(&USARTx_HANDLE, (uint8_t*)data, len, HAL_MAX_DELAY) == HAL_OK)
    {
        return len;
    } else
    {
        return 0;   // 发送失败
    }
}

/* 数据读取接口 */
uint16_t UART_Read(uint8_t* buf, uint16_t max_len)
{
    __disable_irq();

    uint16_t available = (rx_ring.head - rx_ring.tail) & (RING_RX_SIZE - 1);
    if (max_len > available) max_len = available;

    if (max_len > 0)
    {
        uint16_t read_idx = rx_ring.tail & (RING_RX_SIZE - 1);

        if (read_idx + max_len <= RING_RX_SIZE)
        {
            memcpy(buf, &rx_ring.buffer[read_idx], max_len);
        } else
        {
            uint16_t first_part = RING_RX_SIZE - read_idx;
            memcpy(buf, &rx_ring.buffer[read_idx], first_part);
            memcpy(buf + first_part, rx_ring.buffer, max_len - first_part);
        }

        rx_ring.tail += max_len;
    }

    __enable_irq();
    return max_len;
}

/// @brief 初始化蓝牙模块
void BT401_Init(void)
{
    UART_Init();   // 初始化UART和中断接收
}

uint16_t BT401_Read(uint8_t* buffer, uint16_t size)
{
    return UART_Read(buffer, size);   // 读取数据
}

uint16_t BT401_Write(uint8_t* buffer, uint16_t size)
{
    return UART_Write(buffer, size);   // 发送数据
}

uint16_t BT401_Printf(const char* format, ...)
{
    static char fmt_buf[BT401_TX_FORMAT_BUFFER_SIZE];
    va_list args;
    uint16_t len;

    va_start(args, format);
    len = vsnprintf(fmt_buf, sizeof(fmt_buf), format, args);
    va_end(args);

    if (len > sizeof(fmt_buf) - 1)
    {
        len = sizeof(fmt_buf) - 1;
    }

    return BT401_Write((uint8_t*)fmt_buf, len);
}
