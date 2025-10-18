idf_component_register(SRCS "station_example_main.c"
                    PRIV_REQUIRES esp_wifi nvs_flash mbedtls esp_driver_uart esp_driver_gpio aliyun_mqtt
                    INCLUDE_DIRS ".")

littlefs_create_partition_image(storage ../flash_data FLASH_IN_PROJECT)