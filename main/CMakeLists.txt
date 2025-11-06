file(GLOB_RECURSE MAIN_SRC "bsp_aliyun_mqtt/*.c" "bsp_iap/*.c" "bsp_littlefs/*.c" "bsp_uart/*.c" "bsp_wifi_server/*.c" "bsp_usb/*.c" "bsp_bluetooth/*.c")

set(MAIN_INCLUDE_DIRS "." "bsp_aliyun_mqtt" "bsp_iap" "bsp_littlefs" "bsp_uart" "bsp_wifi_server" "bsp_usb" "bsp_bluetooth")

idf_component_register(SRCS "main.c" ${MAIN_SRC}
                    PRIV_REQUIRES esp_wifi nvs_flash mbedtls esp_driver_uart esp_driver_gpio esp_http_server aliyun_mqtt esp_driver_gpio bt
                    INCLUDE_DIRS ${MAIN_INCLUDE_DIRS}
                    EMBED_FILES "bsp_wifi_server/favicon.ico" "bsp_wifi_server/upload_script.html"
                    )

littlefs_create_partition_image(storage ../flash_data FLASH_IN_PROJECT)