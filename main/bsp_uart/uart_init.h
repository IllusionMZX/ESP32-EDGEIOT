#ifndef UART_INIT_H
#define UART_INIT_H

#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"



#define TXD_PIN (GPIO_NUM_4)
#define RXD_PIN (GPIO_NUM_5)
#define RX_BUF_SIZE (1024)

int init_uart(void);
int sendData(const char* logName, const char* data);

#endif // UART_INIT_H