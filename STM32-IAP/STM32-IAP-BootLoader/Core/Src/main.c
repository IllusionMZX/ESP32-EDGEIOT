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
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "bsp_key.h"
#include "bsp_led.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// IAP相关宏定义
#define APPLICATION_ADDRESS     0x08004000  // 应用程序起始地址
#define BOOTLOADER_SIZE         0x4000      // Bootloader大小16KB
#define APP_MAX_SIZE            0xC000      // 应用程序最大48KB

// UART接收相关
#define RX_BUFFER_SIZE          1024
#define IAP_TIMEOUT             5000        // IAP超时5秒

// IAP协议定义
#define IAP_CMD_START           0x55AA      // 开始传输命令
#define IAP_CMD_DATA            0x33CC      // 数据包命令  
#define IAP_CMD_END             0x99FF      // 结束传输命令
#define IAP_ACK                 0xA5        // 应答
#define IAP_NACK                0x5A        // 否定应答

// 特殊标志，用于指示直接启动应用程序
#define JUMP_TO_APP_FLAG        0x12345678  // 魔术数字

// Flash操作返回值
#define FLASH_OK                0
#define FLASH_ERROR             1
#define FLASH_VERIFY_ERROR      2

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
// 用于跳转应用程序的标志变量，存储在RAM中
volatile uint32_t jump_to_app_flag __attribute__((section(".noinit")));

// Banner字符画
const char* banner =
"\r\n"
"  _____ _______ __  __ ____  ___    _____ _          _____  \r\n"
" / ____|__   __|  \\/  |___ \\|__ \\  |_   _| |   /\\   |  __ \\ \r\n"
"| (___    | |  | \\  / | __) |  ) |   | | | |  /  \\  | |__) |\r\n"
" \\___ \\   | |  | |\\/| ||__ <  / /    | | | | / /\\ \\ |  ___/ \r\n"
" ____) |  | |  | |  | |___) |/ /_   _| |_| |/ ____ \\| |     \r\n"
"|_____/   |_|  |_|  |_|____/|____|  |_____|_/    \\_\\_|     \r\n"
"\r\n"
"============================================================\r\n"
"          STM32F103 Bootloader v1.0                       \r\n"
"          UART In-Application Programming                  \r\n"
"============================================================\r\n"
"\r\n";

const char* startup_msg =
"Press any key to enter IAP mode...\r\n"
"Auto boot in %d seconds\r\n";

const char* iap_menu =
"\r\n"
"=== IAP Mode Activated ===\r\n"
"Do you want to upgrade firmware? (yes/no): ";

const char* wait_file_msg =
"\r\n"
"Please send the binary file via UART...\r\n"
"Waiting for file transfer...\r\n";
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void UART_SendString(const char* str)
{
    HAL_UART_Transmit(&huart2, (uint8_t*)str, strlen(str), 1000);
}

// Flash操作函数
uint32_t Flash_Init(void)
{
    HAL_FLASH_Unlock();
    
    // 清除所有错误标志
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR);
    
    return FLASH_OK;
}

void Flash_DeInit(void)
{
    HAL_FLASH_Lock();
}

uint32_t Flash_ErasePage(uint32_t page_addr)
{
    FLASH_EraseInitTypeDef erase_init;
    uint32_t page_error = 0;
    
    Flash_Init();
    
    erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
    erase_init.PageAddress = page_addr;
    erase_init.NbPages = 1;
    
    if (HAL_FLASHEx_Erase(&erase_init, &page_error) != HAL_OK)
    {
        Flash_DeInit();
        return FLASH_ERROR;
    }
    
    Flash_DeInit();
    return FLASH_OK;
}

uint32_t Flash_EraseApplication(void)
{
    uint32_t page_addr;
    uint32_t result = FLASH_OK;
    
    // 擦除应用程序区域（从0x08004000到0x0800FFFF，共48KB，48页）
    for (page_addr = APPLICATION_ADDRESS; page_addr < (APPLICATION_ADDRESS + APP_MAX_SIZE); page_addr += FLASH_PAGE_SIZE)
    {
        if (Flash_ErasePage(page_addr) != FLASH_OK)
        {
            result = FLASH_ERROR;
            break;
        }
    }
    
    return result;
}

uint32_t Flash_WriteData(uint32_t flash_addr, uint8_t* data, uint32_t length)
{
    uint32_t i;
    uint16_t half_word;
    
    Flash_Init();
    
    // STM32F1系列需要按半字（16位）写入
    for (i = 0; i < length; i += 2)
    {
        if (i + 1 < length)
        {
            half_word = data[i] | (data[i + 1] << 8);
        }
        else
        {
            half_word = data[i] | 0xFF00;  // 最后一个字节，高位补0xFF
        }
        
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, flash_addr + i, half_word) != HAL_OK)
        {
            Flash_DeInit();
            return FLASH_ERROR;
        }
        
        // 验证写入的数据
        if (*(uint16_t*)(flash_addr + i) != half_word)
        {
            Flash_DeInit();
            return FLASH_VERIFY_ERROR;
        }
    }
    
    Flash_DeInit();
    return FLASH_OK;
}

// 检查应用程序是否有效
uint8_t IAP_CheckApplication(uint32_t app_addr)
{
    uint32_t stack_addr = *(__IO uint32_t*)app_addr;
    uint32_t reset_addr = *(__IO uint32_t*)(app_addr + 4);

    // 检查栈指针是否在有效范围内
    if ((stack_addr & 0x2FFE0000) == 0x20000000)
    {
        // 检查复位向量是否在Flash范围内
        if ((reset_addr & 0xFF000000) == 0x08000000)
        {
            return 1; // 应用程序有效
        }
    }
    return 0; // 应用程序无效
}

// 跳转到应用程序
void IAP_JumpToApplication(uint32_t app_addr)
{
    // 获取应用程序的栈指针和复位向量
    uint32_t stack_addr = *(volatile uint32_t*)app_addr;
    uint32_t jump_addr = *(volatile uint32_t*)(app_addr + 4);
    
    // 验证栈指针 (应该在SRAM范围内: 0x20000000 - 0x20005000)
    if ((stack_addr < 0x20000000) || (stack_addr > 0x20005000))
    {
        char error_msg[64];
        snprintf(error_msg, sizeof(error_msg), "Invalid stack pointer: 0x%08lX\r\n", stack_addr);
        UART_SendString(error_msg);
        return;
    }
    
    // 验证跳转地址的有效性 (Thumb位必须为1，且在Flash范围内)
    if ((jump_addr & 0x01) == 0 || (jump_addr < 0x08004000) || (jump_addr > 0x08010000))
    {
        char error_msg[64];
        snprintf(error_msg, sizeof(error_msg), "Invalid jump address: 0x%08lX\r\n", jump_addr);
        UART_SendString(error_msg);
        return;
    }
    
    // 发送调试信息
    char debug_msg[128];
    snprintf(debug_msg, sizeof(debug_msg), "Stack: 0x%08lX, Jump: 0x%08lX\r\n", stack_addr, jump_addr);
    UART_SendString(debug_msg);
    UART_SendString("Preparing to jump...\r\n");
    
    // 等待UART发送完成
    HAL_Delay(100);
    
    // 重要：直接跳转，不使用系统复位
    // 这样可以避免复位后的初始化问题
    
    // 关闭所有中断
    __disable_irq();
    
    // 反初始化HAL库和外设
    HAL_DeInit();
    
    
    for (int i = 0; i < 8; i++)
    {
        NVIC->ICPR[i] = 0xFFFFFFFF;  // 清除挂起中断
    }
    
    
    // 设置向量表到应用程序地址
    SCB->VTOR = app_addr;
    
    // 设置主栈指针
    __set_MSP(stack_addr);
    
    // 内存屏障，确保所有写操作完成
    __DSB();
    __ISB();
    
    // 跳转到应用程序
    void (*app_reset_handler)(void) = (void*)jump_addr;
    app_reset_handler();
    
    // 如果到达这里，说明跳转失败
    while(1);
}

// 发送IAP应答
void IAP_SendAck(uint8_t ack)
{
    HAL_UART_Transmit(&huart3, &ack, 1, 1000);
}

// 接收固定长度数据
uint32_t IAP_ReceiveData(uint8_t* buffer, uint32_t length, uint32_t timeout)
{
    return HAL_UART_Receive(&huart3, buffer, length, timeout);
}

// 简单的IAP协议实现
void IAP_ExecuteUpgrade(void)
{
    const char* msg = "IAP Mode: Waiting for firmware...\r\n";
    UART_SendString(msg);
    
    uint8_t buffer[256];  // 数据缓冲区
    uint32_t write_addr = APPLICATION_ADDRESS;
    uint32_t total_received = 0;
    uint16_t cmd;
    uint32_t file_size = 0;
    uint16_t packet_size;
    uint32_t packet_count = 0;
    
    while (1)
    {
        // 接收命令（2字节）
        if (IAP_ReceiveData((uint8_t*)&cmd, 2, 30000) != HAL_OK)
        {
            UART_SendString("Timeout waiting for command\r\n");
            break;
        }
        
        switch (cmd)
        {
            case IAP_CMD_START:
            {
                IAP_SendAck(IAP_ACK);
                UART_SendString("Received START command\r\n");
                UART_SendString("Ready for file size\r\n");
                
                // 接收文件大小（4字节）
                if (IAP_ReceiveData((uint8_t*)&file_size, 4, 5000) != HAL_OK)
                {
                    UART_SendString("Failed to receive file size\r\n");
                    IAP_SendAck(IAP_NACK);
                    break;
                }
                
                char size_msg[64];
                snprintf(size_msg, sizeof(size_msg), "File size: %lu bytes\r\n", file_size);
                UART_SendString(size_msg);
                
                // 检查文件大小
                if (file_size > APP_MAX_SIZE)
                {
                    UART_SendString("File too large!\r\n");
                    IAP_SendAck(IAP_NACK);
                    break;
                }
                
                // 擦除应用程序区域
                UART_SendString("Erasing application area...\r\n");
                if (Flash_EraseApplication() != FLASH_OK)
                {
                    UART_SendString("Flash erase failed!\r\n");
                    IAP_SendAck(IAP_NACK);
                    break;
                }
                
                UART_SendString("Ready to receive data\r\n");
                write_addr = APPLICATION_ADDRESS;
                total_received = 0;
                packet_count = 0;
                IAP_SendAck(IAP_ACK);
                break;
            }
            
            case IAP_CMD_DATA:
            {
                // 接收数据包大小（2字节）
                if (IAP_ReceiveData((uint8_t*)&packet_size, 2, 5000) != HAL_OK)
                {
                    UART_SendString("Failed to receive packet size\r\n");
                    IAP_SendAck(IAP_NACK);
                    break;
                }
                
                // 检查数据包大小
                if (packet_size > sizeof(buffer))
                {
                    UART_SendString("Packet too large!\r\n");
                    IAP_SendAck(IAP_NACK);
                    break;
                }
                
                // 接收数据
                if (IAP_ReceiveData(buffer, packet_size, 5000) != HAL_OK)
                {
                    UART_SendString("Failed to receive data\r\n");
                    IAP_SendAck(IAP_NACK);
                    break;
                }
                
                // 写入Flash
                if (Flash_WriteData(write_addr, buffer, packet_size) != FLASH_OK)
                {
                    UART_SendString("Flash write failed!\r\n");
                    IAP_SendAck(IAP_NACK);
                    break;
                }
                
                write_addr += packet_size;
                total_received += packet_size;
                packet_count++;
                
                // 进度显示（每10包显示一次）
                if ((packet_count % 10) == 0)
                {
                    char progress_msg[64];
                    uint32_t progress = (total_received * 100) / file_size;
                    snprintf(progress_msg, sizeof(progress_msg), "Progress: %lu%%\r\n", progress);
                    UART_SendString(progress_msg);
                }
                
                IAP_SendAck(IAP_ACK);
                break;
            }
            
            case IAP_CMD_END:
            {
                UART_SendString("Received END command\r\n");
                
                // 验证应用程序
                if (IAP_CheckApplication(APPLICATION_ADDRESS))
                {
                    UART_SendString("Firmware upgrade successful!\r\n");
                    IAP_SendAck(IAP_ACK);
                    HAL_Delay(1000);
                    HAL_NVIC_SystemReset();
                }
                else
                {
                    UART_SendString("Invalid application!\r\n");
                    IAP_SendAck(IAP_NACK);
                }
                return;
            }
            
            default:
            {
                IAP_SendAck(IAP_NACK);
                break;
            }
        }
    }
    
    // 升级失败，重启
    UART_SendString("IAP failed, restarting...\r\n");
    HAL_Delay(3000);
    HAL_NVIC_SystemReset();
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
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
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
  
  // 发送启动Banner
  UART_SendString(banner);
  
  // 等待用户输入或超时
  uint8_t rx_char;
  uint32_t timeout = IAP_TIMEOUT;  // 5秒超时
  
  while (timeout > 0)
  {
      // 非阻塞检查串口输入
      if (HAL_UART_Receive(&huart2, &rx_char, 1, 0) == HAL_OK)
      {
          // 用户按了任意键，进入IAP模式
          const char* iap_msg = "\r\nEntering IAP mode...\r\n";
          UART_SendString(iap_msg);
          IAP_ExecuteUpgrade();
          // 升级完成后会自动重启，不会到达这里
      }
      
      // 显示倒计时（每秒更新一次）
      if ((timeout % 1000) == 0)
      {
          char countdown[64];
          snprintf(countdown, sizeof(countdown), "Auto boot in %ld seconds...\r\n", timeout/1000);
          UART_SendString(countdown);
      }
      
      timeout--;
      HAL_Delay(1);  // 1ms延迟
  }
  
  // 超时，检查应用程序
  const char* timeout_msg = "\r\nTimeout! Checking application...\r\n";
  UART_SendString(timeout_msg);
  
  if (IAP_CheckApplication(APPLICATION_ADDRESS))
  {
      const char* jump_msg = "\r\nJumping to application...\r\n";
      UART_SendString(jump_msg);
      HAL_Delay(100);
      
      IAP_JumpToApplication(APPLICATION_ADDRESS);
  }
  else
  {
      const char* no_app_msg = "\r\nNo valid application found!\r\n";
      UART_SendString(no_app_msg);
      const char* force_iap_msg = "Entering IAP mode...\r\n";
      UART_SendString(force_iap_msg);
      
      IAP_ExecuteUpgrade();
  }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
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
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

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
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
