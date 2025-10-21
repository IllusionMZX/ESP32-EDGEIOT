/*
 * bsp_key.h
 *
 *  Created on: Sep 10, 2025
 *      Author: Marti
 */

#ifndef KEY_DRIVER_BSP_KEY_H_
#define KEY_DRIVER_BSP_KEY_H_

typedef enum{
	KEY_NONE = 0,
	KEY_1,
	KEY_2,
	KEY_3,
}KEYS;

KEYS Key_Read();

#endif /* KEY_DRIVER_BSP_KEY_H_ */
