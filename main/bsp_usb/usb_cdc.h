#ifndef _USB_CDC_H_
#define _USB_CDC_H_

#include <stdint.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "tinyusb.h"
#include "tusb_cdc_acm.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

void usb_cdc_init(void);

#ifdef __cplusplus
}
#endif

void usb_cdc_init(void);

#ifdef __cplusplus
}
#endif

#endif /* _USB_CDC_H_ */
