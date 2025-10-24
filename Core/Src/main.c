/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "gpio.h"
#include "rtc.h"
#include "tim.h"
#include "usart.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "MultiTimer.h"
#include "alarm.h"
#include "beep.h"
#include "bt401.h"
#include "hardware_register.h"
#include "key.h"
#include "led.h"
#include "mytime.h"
#include "ntc.h"
#include "pid.h"
#include "protocol.h"
#include "register_interface.h"
#include "shortcut.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
volatile uint64_t platform_ticks = 0;   // 平台滴答计数器
MultiTimer protocolTimer;
MultiTimer alarmTimer;
MultiTimer keyTimer;
MultiTimer ntcTimer;
MultiTimer updateTimer;
MultiTimer countdownTimer;
MultiTimer uploadTimer;
MultiTimer ringTimer;
MultiTimer nightTimer;
MultiTimer queryTimer;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void update_bt_led(void);
void countdown_update(uint32_t decrement);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
uint64_t getPlatformTicks(void)
{
    return platform_ticks;   // 返回平台滴答计数器
}
void protocol_task_callback(MultiTimer* timer, void* arg)
{
    // 协议轮询函数
    protocol_poll();
    multiTimerStart(&protocolTimer, 100, protocol_task_callback, NULL);
}
void alarm_task_callback(MultiTimer* timer, void* arg)
{
    // 闹钟轮询函数
    alarm_poll();
    multiTimerStart(&alarmTimer, 1000, alarm_task_callback, NULL);
}
void key_task_callback(MultiTimer* timer, void* arg)
{
    // 按键扫描函数
    key_scan();
    multiTimerStart(&keyTimer, 20, key_task_callback, NULL);
}
void ntc_task_callback(MultiTimer* timer, void* arg)
{
    // NTC 控温函数
    NTC_control(1000);
    multiTimerStart(&ntcTimer, 1000, ntc_task_callback, NULL);
}
void update_task_callback(MultiTimer* timer, void* arg)
{
    led_update_states();
    beep_update();
    multiTimerStart(&updateTimer, 1, update_task_callback, NULL);   // 每1ms刷新
}
void countdown_task_callback(MultiTimer* timer, void* arg)
{
    // 倒计时更新函数
    countdown_update(1);
    multiTimerStart(&countdownTimer, 1000, countdown_task_callback, NULL);   // 每1000ms更新倒计时
}
void upload_task_callback(MultiTimer* timer, void* arg)
{
    // 上传数据函数
    upload_reg_value();
    multiTimerStart(&uploadTimer, 2000, upload_task_callback, NULL);   // 每3000ms上传数据
}
void ring_task_callback(MultiTimer* timer, void* arg)
{
    // 铃声任务回调函数
    ring_Gradually_increase();
    multiTimerStart(&ringTimer, 500, ring_task_callback, NULL);
}
void night_task_callback(MultiTimer* timer, void* arg)
{
    // 夜间模式任务回调函数
    // Intelligent_temperature_control();
    multiTimerStart(&nightTimer, 60000, night_task_callback, NULL);
}
void query_task_callback(MultiTimer* timer, void* arg)
{
    // print_current_datetime();   // 打印当前时间
    // 查询任务回调函数
    update_bt_led();
    multiTimerStart(&queryTimer, 1000, query_task_callback, NULL);   // 每1000ms查询BLE状态
}
void task_init(void)
{
    multiTimerStart(&updateTimer, 1, update_task_callback, NULL);            // 每1ms刷新
    multiTimerStart(&keyTimer, 20, key_task_callback, NULL);                 // 每20ms扫描按键
    multiTimerStart(&protocolTimer, 100, protocol_task_callback, NULL);      // 每100ms轮询协议
    multiTimerStart(&ntcTimer, 1000, ntc_task_callback, NULL);               // 每1000ms控温
    multiTimerStart(&ringTimer, 500, ring_task_callback, NULL);              // 每500ms轮询铃声
    multiTimerStart(&queryTimer, 1000, query_task_callback, NULL);           // 每1000ms查询BLE状态
    multiTimerStart(&alarmTimer, 1000, alarm_task_callback, NULL);           // 每1000ms轮询闹钟
    multiTimerStart(&countdownTimer, 1000, countdown_task_callback, NULL);   // 每1000ms更新倒计时
    multiTimerStart(&uploadTimer, 2000, upload_task_callback, NULL);         // 每3000ms上传数据
    multiTimerStart(&nightTimer, 60000, night_task_callback, NULL);          // 每60000ms更新夜间模式
}
// 系统初始化
void sys_init(void)
{
    led_init();
    XX_RTC_Init();
    alarm_init();
    shortcut_init();
    Temp_init();
    HAL_TIM_Base_Start_IT(&htim2);
    HAL_TIM_Base_Start_IT(&htim3);
    register_interface_init();   // 初始化寄存器接口
    HAL_Delay(500);
    BT401_Init();                                // 初始化蓝牙模块
    send_at_command("AT+BDLUNAR\r\n", 50);       // 设置蓝牙名称
    send_at_command("AT+BMLUNAR_BLE\r\n", 50);   // 设置ble名称
    send_at_command("AT+CG01\r\n", 50);          // 开启 -- 蓝牙跑后台
    send_at_command("AT+CK00\r\n", 50);          // 关闭 -- 不自动切换至蓝牙
    send_at_command("AT+B200\r\n", 50);          // 关闭蓝牙通话功能
    send_at_command("AT+CR00\r\n", 50);          // 关闭自动回传功能
#ifdef TYPE_PILLOW_NORMAL
    send_at_command("AT+CP01\r\n", 50);   // 上电进入等待状态，需要用户发送模式指令
#endif
#ifdef TYPE_PILLOW_U
    mode_control(BLUETOOTH_MODE);   // 上电直接进入蓝牙模式
#endif
}
int main(void)
{

    /* USER CODE BEGIN 1 */

    /* USER CODE END 1 */

    /* MCU Configuration--------------------------------------------------------*/

    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
    HAL_Init();

    /* USER CODE BEGIN Init */

    /* USER CODE END Init */

    /* Configure the system clock */
    SystemClock_Config();

    /* USER CODE BEGIN SysInit */

    /* USER CODE END SysInit */

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_RTC_Init();
    MX_USART3_UART_Init();
    MX_TIM1_Init();
    MX_TIM2_Init();
    MX_TIM3_Init();
    MX_ADC1_Init();
    // MX_USART2_UART_Init();
    /* USER CODE BEGIN 2 */
    HAL_GPIO_WritePin(POWER_GPIO_Port, POWER_Pin, GPIO_PIN_SET);     // 设置电源引脚为高电平
    HAL_GPIO_WritePin(LED_B_GPIO_Port, LED_B_Pin, GPIO_PIN_RESET);   // Turn on the blue LED
    HAL_Delay(10);                                                   // 延时10ms，等待系统稳定
    sys_init();                                                      // 初始化系统
    HAL_Delay(10);                                                   // 延时10ms，等待硬件初始化完成

    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    multiTimerInstall(getPlatformTicks);   // 安装平台滴答计数器获取函数
    task_init();
    HAL_Delay(500);
    while (1)
    {
        multiTimerYield();   // 执行多定时器的回调函数
        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */
    }
    /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct   = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct   = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    /** Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     */
    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSE;
    RCC_OscInitStruct.LSEState            = RCC_LSE_ON;
    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_HSI_DIV2;
    RCC_OscInitStruct.PLL.PLLMUL          = RCC_PLL_MUL16;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        // LSE配置失败，切换为LSI
        RCC_OscInitStruct.LSEState = RCC_LSE_OFF;   // 关闭LSE
        RCC_OscInitStruct.OscillatorType |= RCC_OSCILLATORTYPE_LSI;
        RCC_OscInitStruct.LSIState = RCC_LSI_ON;

        if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
        {
            Error_Handler();   // 如果连LSI都无法启用，则进入错误处理
        }
    }

    /** Initializes the CPU, AHB and APB buses clocks
     */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }

    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC | RCC_PERIPHCLK_ADC;
    // 如果当前使用的是LSI，则设置RTC时钟源为LSI，否则仍使用LSE
    if (RCC_OscInitStruct.LSIState == RCC_LSI_ON)
    {
        PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
    } else
    {
        PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
    }
    PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV8;

    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
        Error_Handler();
    }
}
/* USER CODE BEGIN 4 */

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim)
{
    if (htim->Instance == TIM2)
    {
        beep_pwm_update();
    }
    if (htim->Instance == TIM3)
    {
        platform_ticks++;   // 平台滴答计数器自增
    }
}
void update_bt_led(void)
{
    if (ble_key_pressed)
    {

        uint8_t status = query_ble_status();   // 查询蓝牙状态
        if (status == 0)
        {
            led_set_mode(LED_BT, LED_MODE_BLINK, 500);
        } else
        {
            led_set_mode(LED_BT, LED_MODE_ON, 0);
        }
    } else
    {
        led_set_mode(LED_BT, LED_MODE_OFF, 0);
    }
}

void two_hour_protect(void)
{
    static uint32_t heat_protect_time = 0;   // 热敷最长使用时间
    if (is_heating_active() && get_remaining_seconds() == 0)
    {
        heat_protect_time += 1;   // 每次调用增加1s
    } else
    {
        heat_protect_time = 0;   // 如果不在加热状态，重置计时
    }
    if (heat_protect_time >= 7200)   // 达到2小时（7200秒）时触发保护
    {
        stop_heating_task();
        heat_protect_time = 0;   // 重置计时
    }
}
void countdown_update(uint32_t decrement)
{
    two_hour_protect();
    uint32_t remaining_seconds = get_remaining_seconds();
    // 更新 LED 时间选择
    uint16_t remaining_minutes = remaining_seconds / 60;
    led_time_select(remaining_minutes);

    // 如果剩余时间大于0且加热或音乐任务正在运行
    if (remaining_seconds > 0 && (is_heating_active() || is_music_active()))
    {
        remaining_seconds -= decrement;
        if (remaining_seconds < 1) remaining_seconds = 0;

        set_remaining_seconds(remaining_seconds);

        // 更新寄存器（仅在整分钟或归零时）
        if ((remaining_seconds % 60 == 0) || (remaining_seconds == 0))
        {
            register_set_value(REG_HEATING_TIMER, remaining_seconds / 60);
        }

        // 停止加热与音乐任务
        if (remaining_seconds == 0)
        {
            reset_act_shortcut_id(0);
            stop_heating_task();
            stop_music_task();
        }
    }
}

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
    /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */
    __disable_irq();
    while (1)
    {
        // 发生错误时，LED闪烁
        led_set_mode(LED_B, LED_MODE_BLINK, 200);   // 红色LED闪烁500ms
        led_update_states();
    }
    /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t* file, uint32_t line)
{
    /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line number,
       ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
