#ifndef STM32_IAP_H
#define STM32_IAP_H

#include "esp_err.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include <sys/stat.h>

// IAP协议定义
#define IAP_CMD_START 0x55AA
#define IAP_CMD_DATA  0x33CC
#define IAP_CMD_END   0x99FF
#define IAP_ACK       0xA5
#define IAP_NACK      0x5A

bool send_firmware_via_uart(const char *file_path);


#endif // STM32_IAP_H