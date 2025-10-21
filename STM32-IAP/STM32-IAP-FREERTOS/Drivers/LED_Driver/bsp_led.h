/*
 * bsp_led.h
 *
 *  Created on: Sep 10, 2025
 *      Author: Marti
 */

#ifndef LED_DRIVER_BSP_LED_H_
#define LED_DRIVER_BSP_LED_H_
#include "main.h"

#define LED_BLUE_Toggle() HAL_GPIO_TogglePin(LED_BLUE_GPIO_Port,LED_BLUE_Pin)
#define LED_RED_Toggle() HAL_GPIO_TogglePin(LED_RED_GPIO_Port,LED_RED_Pin)
#define LED_GREEN_Toggle() HAL_GPIO_TogglePin(LED_GREEN_GPIO_Port,LED_GREEN_Pin)

#define LED_BLUE_On() HAL_GPIO_WritePin(LED_BLUE_GPIO_Port,LED_BLUE_Pin,GPIO_PIN_SET)
#define LED_RED_On() HAL_GPIO_WritePin(LED_RED_GPIO_Port,LED_RED_Pin,GPIO_PIN_SET)
#define LED_GREEN_On() HAL_GPIO_WritePin(LED_GREEN_GPIO_Port,LED_GREEN_Pin,GPIO_PIN_SET)

#define LED_BLUE_Off() HAL_GPIO_WritePin(LED_BLUE_GPIO_Port,LED_BLUE_Pin,GPIO_PIN_RESET)
#define LED_RED_Off() HAL_GPIO_WritePin(LED_RED_GPIO_Port,LED_RED_Pin,GPIO_PIN_RESET)
#define LED_GREEN_Off() HAL_GPIO_WritePin(LED_GREEN_GPIO_Port,LED_GREEN_Pin,GPIO_PIN_RESET)

#endif /* LED_DRIVER_BSP_LED_H_ */
