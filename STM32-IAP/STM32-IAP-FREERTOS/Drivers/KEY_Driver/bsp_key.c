/*
 * bsp_key.c
 *
 *  Created on: Sep 10, 2025
 *      Author: Marti
 */

#include "main.h"
#include "bsp_key.h"




KEYS Key_Read()
{
	if(HAL_GPIO_ReadPin(KEY1_GPIO_Port,KEY1_Pin)==GPIO_PIN_RESET)
	{
		return KEY_1;
	} else if(HAL_GPIO_ReadPin(KEY2_GPIO_Port,KEY2_Pin)==GPIO_PIN_RESET)
	{
		return KEY_2;
	} else if(HAL_GPIO_ReadPin(KEY3_GPIO_Port,KEY3_Pin)==GPIO_PIN_RESET)
	{
		return KEY_3;
	}else
	{
		return KEY_NONE;
	}
}
