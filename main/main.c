#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "bsp_uart/uart_init.h"
#include "bsp_littlefs/littlefs_mount.h"
#include "bsp_wifi_server/wifi_init.h"
#include "bsp_aliyun_mqtt/aliyun_mqtt.h"
#include "bsp_wifi_server/file_server.h"
#include "bsp_usb/usb_cdc.h"
#include "bsp_bluetooth/bsp_nimble_led.h"

#ifdef __cplusplus
}
#endif

static const char *TAG = "iot_platform_example_main";

void app_main(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();
    
    const char* base_path = "/littlefs";

    ESP_LOGI(TAG, "Initializing LittleFS");
    mount_storage_littlefs(base_path);

    ESP_LOGI(TAG, "UART1 initialization");
    init_uart();

    /* start usb cdc */
    ESP_LOGI(TAG, "Starting USB CDC");
    usb_cdc_init();

    /* start linkkit mqtt */
    ESP_LOGI(TAG, "Start linkkit mqtt");
    linkkit_main();

    /* start nimble led */
    ESP_LOGI(TAG, "Starting NimBLE LED");
    bsp_nimble_led_init();

    /* start file server */
    ESP_LOGI(TAG, "Starting file server");
    example_start_file_server(base_path);
}