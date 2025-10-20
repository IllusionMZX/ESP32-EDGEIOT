#ifndef ALIYUN_MQTT_H
#define ALIYUN_MQTT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "aiot_state_api.h"
#include "aiot_sysdep_api.h"
#include "aiot_mqtt_api.h"
#include "aiot_ota_api.h"
#include "aiot_mqtt_download_api.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"


#include "esp_log.h"
#include "esp_err.h"
#include "driver/uart.h"

#include "bsp_iap/stm32_iap.h"

int linkkit_main(void);

#endif // ALIYUN_MQTT_H