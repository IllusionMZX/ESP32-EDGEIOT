/* WiFi station Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#ifdef __cplusplus
extern "C" {
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi_types_generic.h"
#include "esp_system.h"
#include "esp_littlefs.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

#include "aiot_state_api.h"
#include "aiot_sysdep_api.h"
#include "aiot_mqtt_api.h"
#include "aiot_ota_api.h"
#include "aiot_mqtt_download_api.h"

#ifdef __cplusplus
}
#endif


/* The examples use WiFi configuration that you can set via project configuration menu

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1


#define TXD_PIN (GPIO_NUM_4)
#define RXD_PIN (GPIO_NUM_5)
#define RX_BUF_SIZE (1024)


// IAP协议定义
#define IAP_CMD_START 0x55AA
#define IAP_CMD_DATA  0x33CC
#define IAP_CMD_END   0x99FF
#define IAP_ACK       0xA5
#define IAP_NACK      0x5A

bool send_firmware_via_uart(const char *file_path);

static const char *TAG = "wifi station";

static int s_retry_num = 0;

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {

            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler));
    // ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler));
    vEventGroupDelete(s_wifi_event_group);
}


/*
 * 这个例程适用于`Linux`这类支持pthread的POSIX设备, 它演示了用SDK配置MQTT参数并建立连接, 之后创建2个线程
 *
 * + 一个线程用于保活长连接
 * + 一个线程用于接收消息, 并在有消息到达时进入默认的数据回调, 在连接状态变化时进入事件回调
 *
 * 需要用户关注或修改的部分, 已经用 TODO 在注释中标明
 *
 */

/* TODO: 替换为自己设备的三元组 */
char *product_key       = "k1xu1764jnp";
char *device_name       = "ESP32S3";
char *device_secret     = "56a372381ffbacf288327607e9350385";

/* 位于portfiles/aiot_port文件夹下的系统适配函数集合 */
extern aiot_sysdep_portfile_t g_aiot_sysdep_portfile;

/* 位于external/ali_ca_cert.c中的服务器证书 */
extern const char *ali_ca_cert;

static TaskHandle_t g_mqtt_process_task;
static TaskHandle_t g_mqtt_recv_task;
static uint8_t g_mqtt_process_task_running = 0;
static uint8_t g_mqtt_recv_task_running = 0;

void *g_ota_handle = NULL;
void *g_dl_handle = NULL;
uint32_t g_firmware_size = 0;
static char g_new_version[32] = {0};

/* TODO: 如果要关闭日志, 就把这个函数实现为空, 如果要减少日志, 可根据code选择不打印
 *
 * 例如: [1577589489.033][LK-0317] mqtt_basic_demo&a13FN5TplKq
 *
 * 上面这条日志的code就是0317(十六进制), code值的定义见core/aiot_state_api.h
 *
 */

/* 日志回调函数, SDK的日志会从这里输出 */
int32_t demo_state_logcb(int32_t code, char *message)
{
    printf("%s", message);
    return 0;
}

/* MQTT事件回调函数, 当网络连接/重连/断开时被触发, 事件定义见core/aiot_mqtt_api.h */
void demo_mqtt_event_handler(void *handle, const aiot_mqtt_event_t *event, void *userdata)
{
    switch (event->type) {
        /* SDK因为用户调用了aiot_mqtt_connect()接口, 与mqtt服务器建立连接已成功 */
        case AIOT_MQTTEVT_CONNECT: {
            printf("AIOT_MQTTEVT_CONNECT\n");
            /* TODO: 处理SDK建连成功, 不可以在这里调用耗时较长的阻塞函数 */
        }
        break;

        /* SDK因为网络状况被动断连后, 自动发起重连已成功 */
        case AIOT_MQTTEVT_RECONNECT: {
            printf("AIOT_MQTTEVT_RECONNECT\n");
            /* TODO: 处理SDK重连成功, 不可以在这里调用耗时较长的阻塞函数 */
        }
        break;

        /* SDK因为网络的状况而被动断开了连接, network是底层读写失败, heartbeat是没有按预期得到服务端心跳应答 */
        case AIOT_MQTTEVT_DISCONNECT: {
            const char *cause = (event->data.disconnect == AIOT_MQTTDISCONNEVT_NETWORK_DISCONNECT) ? ("network disconnect") :
                          ("heartbeat disconnect");
            printf("AIOT_MQTTEVT_DISCONNECT: %s\n", cause);
            /* TODO: 处理SDK被动断连, 不可以在这里调用耗时较长的阻塞函数 */
        }
        break;

        default: {

        }
    }
}

/* MQTT默认消息处理回调, 当SDK从服务器收到MQTT消息时, 且无对应用户回调处理时被调用 */
void demo_mqtt_default_recv_handler(void *handle, const aiot_mqtt_recv_t *packet, void *userdata)
{
    switch (packet->type) {
        case AIOT_MQTTRECV_HEARTBEAT_RESPONSE: {
            printf("heartbeat response\n");
            /* TODO: 处理服务器对心跳的回应, 一般不处理 */
        }
        break;

        case AIOT_MQTTRECV_SUB_ACK: {
            printf("suback, res: -0x%04lX, packet id: %d, max qos: %d\n",
                   (unsigned long)-packet->data.sub_ack.res, packet->data.sub_ack.packet_id, packet->data.sub_ack.max_qos);
            /* TODO: 处理服务器对订阅请求的回应, 一般不处理 */
        }
        break;

        case AIOT_MQTTRECV_PUB: {
            printf("pub, qos: %d, topic: %.*s\n", packet->data.pub.qos, packet->data.pub.topic_len, packet->data.pub.topic);
            printf("pub, payload: %.*s\n", (int)packet->data.pub.payload_len, packet->data.pub.payload);
            /* TODO: 处理服务器下发的业务报文 */
        }
        break;

        case AIOT_MQTTRECV_PUB_ACK: {
            printf("puback, packet id: %d\n", packet->data.pub_ack.packet_id);
            /* TODO: 处理服务器对QoS1上报消息的回应, 一般不处理 */
        }
        break;

        default: {

        }
    }
}

/* 执行aiot_mqtt_process的线程, 包含心跳发送和QoS1消息重发 */
void demo_mqtt_process_task(void *args)
{
    int32_t res = STATE_SUCCESS;

    while (g_mqtt_process_task_running) {
        res = aiot_mqtt_process(args);
        if (res == STATE_USER_INPUT_EXEC_DISABLED) {
            break;
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

/* 执行aiot_mqtt_recv的线程, 包含网络自动重连和从服务器收取MQTT消息 */
void demo_mqtt_recv_task(void *args)
{
    int32_t res = STATE_SUCCESS;

    while (g_mqtt_recv_task_running) {
        res = aiot_mqtt_recv(args);
        if (res < STATE_SUCCESS) {
            if (res == STATE_USER_INPUT_EXEC_DISABLED) {
                break;
            }
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }
    vTaskDelete(NULL);
}

/* 执行OTA下载处理的线程 */
void demo_ota_process_task(void *args)
{
    while (1) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        if(g_dl_handle != NULL) {
            int32_t res = aiot_mqtt_download_process(g_dl_handle);

            if(STATE_MQTT_DOWNLOAD_SUCCESS == res) {
                /* 升级成功，这里重启并且上报新的版本号 */
                printf("mqtt download ota success \r\n");
                aiot_mqtt_download_deinit(&g_dl_handle);
                                
                // 新增：通过UART1发送固件给STM32，支持重试
                int retry_count = 0;
                const int max_retries = 3;
                bool send_success = false;
                while (retry_count < max_retries) {
                    if (send_firmware_via_uart("/littlefs/STM32-IAP-FREERTOS.bin")) {
                        ESP_LOGI("IAP", "Firmware sent to STM32 successfully");
                        send_success = true;
                        break;
                    } else {
                        ESP_LOGE("IAP", "Failed to send firmware to STM32, retry %d/%d", retry_count + 1, max_retries);
                        retry_count++;
                        if (retry_count < max_retries) {
                            vTaskDelay(pdMS_TO_TICKS(5000));  // 等待5秒后重试
                        }
                    }
                }
                if (!send_success) {
                    ESP_LOGE("IAP", "All retries failed, firmware not sent to STM32");
                }

                // 上报新版本号
                if (g_new_version[0] != '\0') {
                    int32_t res_report = aiot_ota_report_version(g_ota_handle, g_new_version);
                    if (res_report < STATE_SUCCESS) {
                        printf("report new version failed, code is -0x%04lX\r\n", (unsigned long)-res_report);
                    } else {
                        printf("reported new version: %s\r\n", g_new_version);
                    }
                }
                // TODO: 处理升级成功后的逻辑，如重启设备
            } else if(STATE_MQTT_DOWNLOAD_FAILED_RECVERROR == res
                      || STATE_MQTT_DOWNLOAD_FAILED_TIMEOUT == res
                      || STATE_MQTT_DOWNLOAD_FAILED_MISMATCH == res) {
                printf("mqtt download ota failed \r\n");
                aiot_mqtt_download_deinit(&g_dl_handle);
            }
        }
    }
}

/* 下载收包回调, 用户调用 aiot_download_recv() 后, SDK收到数据会进入这个函数, 把下载到的数据交给用户 */
/* TODO: 一般来说, 设备升级时, 会在这个回调中, 把下载到的数据写到Flash上 */
void user_download_recv_handler(void *handle, const aiot_mqtt_download_recv_t *packet, void *userdata)
{
    uint32_t data_buffer_len = 0;

    /* 目前只支持 packet->type 为 AIOT_MDRECV_DATA_RESP 的情况 */
    if (!packet || AIOT_MDRECV_DATA_RESP != packet->type) {
        return;
    }

    /* 用户应在此实现文件本地固化的操作 */
    FILE *file = fopen("/littlefs/STM32-IAP-FREERTOS.bin", "ab");
    fwrite(packet->data.data_resp.data, packet->data.data_resp.data_size, sizeof(int8_t), file);
    fclose(file);

    data_buffer_len = packet->data.data_resp.data_size;

    printf("download %03ld%% done, +%lu bytes\r\n", (long)packet->data.data_resp.percent, (unsigned long)data_buffer_len);
}

/* 用户通过 aiot_ota_setopt() 注册的OTA消息处理回调, 如果SDK收到了OTA相关的MQTT消息, 会自动识别, 调用这个回调函数 */
void user_ota_recv_handler(void *ota_handle, aiot_ota_recv_t *ota_msg, void *userdata)
{
    uint32_t request_size = 10 * 1024;
    switch (ota_msg->type) {
    case AIOT_OTARECV_FOTA: {
        if (NULL == ota_msg->task_desc || ota_msg->task_desc->protocol_type != AIOT_OTA_PROTOCOL_MQTT) {
            break;
        }

        if(g_dl_handle != NULL) {
            aiot_mqtt_download_deinit(&g_dl_handle);
        }

        printf("OTA target firmware version: %s, size: %lu Bytes\r\n", ota_msg->task_desc->version,
               (unsigned long)ota_msg->task_desc->size_total);
        snprintf(g_new_version, sizeof(g_new_version), "%s", ota_msg->task_desc->version);

        void *md_handler = aiot_mqtt_download_init();
        aiot_mqtt_download_setopt(md_handler, AIOT_MDOPT_TASK_DESC, ota_msg->task_desc);
        /* 设置下载一包的大小，对于资源受限设备可以调整该值大小 */
        aiot_mqtt_download_setopt(md_handler, AIOT_MDOPT_DATA_REQUEST_SIZE, &request_size);

        /* 部分场景下，用户如果只需要下载文件的一部分，即下载指定range的文件，可以设置文件起始位置、终止位置。
         * 若设置range区间下载，单包报文的数据有CRC校验，但SDK将不进行完整文件MD5校验，
         * 默认下载全部文件，单包报文的数据有CRC校验，并且SDK会对整个文件进行md5校验 */
        // uint32_t range_start = 10, range_end = 50 * 1024 + 10;
        // aiot_mqtt_download_setopt(md_handler, AIOT_MDOPT_RANGE_START, &range_start);
        // aiot_mqtt_download_setopt(md_handler, AIOT_MDOPT_RANGE_END, &range_end);

        aiot_mqtt_download_setopt(md_handler, AIOT_MDOPT_RECV_HANDLE, user_download_recv_handler);
        g_dl_handle = md_handler;
    }
    break;
    default:
        break;
    }
}

int sendData(const char* logName, const char* data)
{
    const int len = strlen(data);
    const int txBytes = uart_write_bytes(UART_NUM_1, data, len);
    ESP_LOGI(logName, "Wrote %d bytes", txBytes);
    return txBytes;
}

// static void tx_task(void *arg)
// {
//     static const char *TX_TASK_TAG = "TX_TASK";
//     esp_log_level_set(TX_TASK_TAG, ESP_LOG_INFO);
//     while (1) {
//         sendData(TX_TASK_TAG, "Hello world");
//         vTaskDelay(2000 / portTICK_PERIOD_MS);
//     }
// }

// static void rx_task(void *arg)
// {
//     static const char *RX_TASK_TAG = "RX_TASK";
//     esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
//     uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE + 1);
//     while (1) {
//         const int rxBytes = uart_read_bytes(UART_NUM_1, data, RX_BUF_SIZE, 1000 / portTICK_PERIOD_MS);
//         if (rxBytes > 0) {
//             data[rxBytes] = 0;
//             ESP_LOGI(RX_TASK_TAG, "Read %d bytes: '%s'", rxBytes, data);
//             ESP_LOG_BUFFER_HEXDUMP(RX_TASK_TAG, data, rxBytes, ESP_LOG_INFO);
//         }
//     }
//     free(data);
// }

int linkkit_main(void)
{
    int32_t     res = STATE_SUCCESS;
    void       *mqtt_handle = NULL;
    char       *url = "iot-as-mqtt.cn-shanghai.aliyuncs.com"; /* 阿里云平台上海站点的域名后缀 */
    char        host[100] = {0}; /* 用这个数组拼接设备连接的云平台站点全地址, 规则是 ${productKey}.iot-as-mqtt.cn-shanghai.aliyuncs.com */
    uint16_t    port = 443;      /* 无论设备是否使用TLS连接阿里云平台, 目的端口都是443 */
    aiot_sysdep_network_cred_t cred; /* 安全凭据结构体, 如果要用TLS, 这个结构体中配置CA证书等参数 */

    /* 配置SDK的底层依赖 */
    aiot_sysdep_set_portfile(&g_aiot_sysdep_portfile);
    /* 配置SDK的日志输出 */
    aiot_state_set_logcb(demo_state_logcb);

    /* 创建SDK的安全凭据, 用于建立TLS连接 */
    memset(&cred, 0, sizeof(aiot_sysdep_network_cred_t));
    cred.option = AIOT_SYSDEP_NETWORK_CRED_SVRCERT_CA;  /* 使用RSA证书校验MQTT服务端 */
    cred.max_tls_fragment = 16384; /* 最大的分片长度为16K, 其它可选值还有4K, 2K, 1K, 0.5K */
    cred.sni_enabled = 1;                               /* TLS建连时, 支持Server Name Indicator */
    cred.x509_server_cert = ali_ca_cert;                 /* 用来验证MQTT服务端的RSA根证书 */
    cred.x509_server_cert_len = strlen(ali_ca_cert);     /* 用来验证MQTT服务端的RSA根证书长度 */

    /* 创建1个MQTT客户端实例并内部初始化默认参数 */
    mqtt_handle = aiot_mqtt_init();
    if (mqtt_handle == NULL) {
        printf("aiot_mqtt_init failed\n");
        return -1;
    }

    /* TODO: 如果以下代码不被注释, 则例程会用TCP而不是TLS连接云平台 */
    /*
    {
        memset(&cred, 0, sizeof(aiot_sysdep_network_cred_t));
        cred.option = AIOT_SYSDEP_NETWORK_CRED_NONE;
    }
    */

    snprintf(host, 100, "%s.%s", product_key, url);
    /* 配置MQTT服务器地址 */
    aiot_mqtt_setopt(mqtt_handle, AIOT_MQTTOPT_HOST, (void *)host);
    /* 配置MQTT服务器端口 */
    aiot_mqtt_setopt(mqtt_handle, AIOT_MQTTOPT_PORT, (void *)&port);
    /* 配置设备productKey */
    aiot_mqtt_setopt(mqtt_handle, AIOT_MQTTOPT_PRODUCT_KEY, (void *)product_key);
    /* 配置设备deviceName */
    aiot_mqtt_setopt(mqtt_handle, AIOT_MQTTOPT_DEVICE_NAME, (void *)device_name);
    /* 配置设备deviceSecret */
    aiot_mqtt_setopt(mqtt_handle, AIOT_MQTTOPT_DEVICE_SECRET, (void *)device_secret);
    /* 配置网络连接的安全凭据, 上面已经创建好了 */
    aiot_mqtt_setopt(mqtt_handle, AIOT_MQTTOPT_NETWORK_CRED, (void *)&cred);
    /* 配置MQTT默认消息接收回调函数 */
    aiot_mqtt_setopt(mqtt_handle, AIOT_MQTTOPT_RECV_HANDLER, (void *)demo_mqtt_default_recv_handler);
    /* 配置MQTT事件回调函数 */
    aiot_mqtt_setopt(mqtt_handle, AIOT_MQTTOPT_EVENT_HANDLER, (void *)demo_mqtt_event_handler);

    /* 与服务器建立MQTT连接 */
    res = aiot_mqtt_connect(mqtt_handle);
    if (res < STATE_SUCCESS) {
        /* 尝试建立连接失败, 销毁MQTT实例, 回收资源 */
        aiot_mqtt_deinit(&mqtt_handle);
        printf("aiot_mqtt_connect failed: -0x%04lX\n", (unsigned long)-res);
        return -1;
    }

    /* 与MQTT例程不同的是, 这里需要增加创建OTA会话实例的语句 */
    void *ota_handle = aiot_ota_init();
    if (NULL == ota_handle) {
        goto exit;
    }

    /* 用以下语句, 把OTA会话和MQTT会话关联起来 */
    aiot_ota_setopt(ota_handle, AIOT_OTAOPT_MQTT_HANDLE, mqtt_handle);
    /* 用以下语句, 设置OTA会话的数据接收回调, SDK收到OTA相关推送时, 会进入这个回调函数 */
    aiot_ota_setopt(ota_handle, AIOT_OTAOPT_RECV_HANDLER, user_ota_recv_handler);
    g_ota_handle = ota_handle;

    char *cur_version = "1.0.1";
    /* 演示MQTT连接建立起来之后, 就可以上报当前设备的版本号了 */
    res = aiot_ota_report_version(ota_handle, cur_version);
    if (res < STATE_SUCCESS) {
        printf("report version failed, code is -0x%04lX\r\n", (unsigned long)-res);
        goto exit;
    }

    /* 创建一个单独的线程, 专用于执行aiot_mqtt_process, 它会自动发送心跳保活, 以及重发QoS1的未应答报文 */
    g_mqtt_process_task_running = 1;
    BaseType_t ret = xTaskCreate(demo_mqtt_process_task, "mqtt_process", 4096, mqtt_handle, 5, &g_mqtt_process_task);
    if (ret != pdPASS) {
        printf("xTaskCreate demo_mqtt_process_task failed: %ld\n", (long)ret);
        goto exit;
    }

    /* 创建一个单独的线程用于执行aiot_mqtt_recv, 它会循环收取服务器下发的MQTT消息, 并在断线时自动重连 */
    g_mqtt_recv_task_running = 1;
    ret = xTaskCreate(demo_mqtt_recv_task, "mqtt_recv", 4096, mqtt_handle, 5, &g_mqtt_recv_task);
    if (ret != pdPASS) {
        printf("xTaskCreate demo_mqtt_recv_task failed: %ld\n", (long)ret);
        goto exit;
    }

    /* 创建一个单独的线程用于执行OTA下载处理 */
    ret = xTaskCreate(demo_ota_process_task, "ota_process", 4096, NULL, 5, NULL);
    if (ret != pdPASS) {
        printf("xTaskCreate demo_ota_process_task failed: %ld\n", (long)ret);
        goto exit;
    }

    // /* 创建UART发送任务 */
    // ret = xTaskCreate(tx_task, "tx_task", 2048, NULL, 5, NULL);
    // if (ret != pdPASS) {
    //     printf("xTaskCreate tx_task failed: %ld\n", (long)ret);
    //     goto exit;
    // }

    // /* 创建UART接收任务 */
    // ret = xTaskCreate(rx_task, "rx_task", 4096, NULL, 5, NULL);
    // if (ret != pdPASS) {
    //     printf("xTaskCreate rx_task failed: %ld\n", (long)ret);
    //     goto exit;
    // }

    /* 任务创建成功，返回 */
    return 0;

exit:
    /* 断开MQTT连接 */
    if (mqtt_handle) {
        aiot_mqtt_disconnect(mqtt_handle);
        aiot_mqtt_deinit(&mqtt_handle);
    }
    /* 销毁OTA实例 */
    if (ota_handle) {
        aiot_ota_deinit(&ota_handle);
    }

    g_mqtt_process_task_running = 0;
    g_mqtt_recv_task_running = 0;

    return -1;
}

// 发送命令
void send_command(uart_port_t uart_num, uint16_t cmd) {
    uint8_t cmd_bytes[2];
    cmd_bytes[0] = cmd & 0xFF;
    cmd_bytes[1] = (cmd >> 8) & 0xFF;
    uart_write_bytes(uart_num, (const char*)cmd_bytes, 2);
    ESP_LOGI("IAP", "Sent command: 0x%04X", cmd);
}

// 等待ACK/NACK
bool wait_ack(uart_port_t uart_num, TickType_t timeout_ticks) {
    uint8_t data;
    TickType_t start = xTaskGetTickCount();
    while ((xTaskGetTickCount() - start) < timeout_ticks) {
        int len = uart_read_bytes(uart_num, &data, 1, pdMS_TO_TICKS(10));
        if (len > 0) {
            if (data == IAP_ACK) {
                ESP_LOGI("IAP", "Received ACK");
                return true;
            } else if (data == IAP_NACK) {
                ESP_LOGI("IAP", "Received NACK");
                return false;
            }
        }
    }
    ESP_LOGW("IAP", "ACK timeout");
    return false;
}

// 等待STM32准备消息
bool wait_for_ready_message(uart_port_t uart_num, TickType_t timeout_ticks) {
    char buffer[256];
    TickType_t start = xTaskGetTickCount();
    while ((xTaskGetTickCount() - start) < timeout_ticks) {
        int len = uart_read_bytes(uart_num, (uint8_t*)buffer, sizeof(buffer) - 1, pdMS_TO_TICKS(100));
        if (len > 0) {
            buffer[len] = '\0';
            ESP_LOGI("IAP", "Received: %s", buffer);
            if (strstr(buffer, "Ready for file size")) {
                return true;
            }
        }
    }
    return false;
}

// 通过UART发送固件
bool send_firmware_via_uart(const char *file_path) {
    FILE *file = fopen(file_path, "rb");
    if (!file) {
        ESP_LOGE("IAP", "Failed to open file: %s", file_path);
        return false;
    }

    // 获取文件大小
    fseek(file, 0, SEEK_END);
    uint32_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    ESP_LOGI("IAP", "File size: %lu bytes", file_size);

    // 清空UART缓冲区
    uart_flush(UART_NUM_1);

    // 1. 发送开始命令并等待ACK
    send_command(UART_NUM_1, IAP_CMD_START);
    if (!wait_ack(UART_NUM_1, pdMS_TO_TICKS(15000))) {  // 匹配Python的timeout=15
        fclose(file);
        return false;
    }

    // 发送文件大小
    uint8_t size_bytes[4];
    size_bytes[0] = file_size & 0xFF;
    size_bytes[1] = (file_size >> 8) & 0xFF;
    size_bytes[2] = (file_size >> 16) & 0xFF;
    size_bytes[3] = (file_size >> 24) & 0xFF;
    uart_write_bytes(UART_NUM_1, (const char*)size_bytes, 4);
    ESP_LOGI("IAP", "Sent file size: %lu", file_size);

    if (!wait_ack(UART_NUM_1, pdMS_TO_TICKS(15000))) {
        fclose(file);
        return false;
    }

    // 2. 分包发送数据（每包128字节）
    uint8_t packet[128];
    uint32_t total_packets = (file_size + 127) / 128;
    for (uint32_t i = 0; i < total_packets; i++) {
        size_t packet_size = fread(packet, 1, 128, file);
        if (packet_size == 0) break;

        // 发送数据命令
        send_command(UART_NUM_1, IAP_CMD_DATA);
        vTaskDelay(pdMS_TO_TICKS(10));  // 小延迟，匹配Python的time.sleep(0.01)

        // 发送包大小
        uint8_t size_buf[2];
        size_buf[0] = packet_size & 0xFF;
        size_buf[1] = (packet_size >> 8) & 0xFF;
        uart_write_bytes(UART_NUM_1, (const char*)size_buf, 2);
        vTaskDelay(pdMS_TO_TICKS(10));

        // 发送数据
        uart_write_bytes(UART_NUM_1, (const char*)packet, packet_size);

        ESP_LOGI("IAP", "Sent packet %lu/%lu (%zu bytes)", i + 1, total_packets, packet_size);

        if (!wait_ack(UART_NUM_1, pdMS_TO_TICKS(10000))) {  // 默认timeout=10秒
            fclose(file);
            return false;
        }

        vTaskDelay(pdMS_TO_TICKS(10));  // 适当延迟
    }

    // 3. 发送结束命令
    send_command(UART_NUM_1, IAP_CMD_END);
    bool success = wait_ack(UART_NUM_1, pdMS_TO_TICKS(10000));

    fclose(file);
    return success;
}

int init_uart(void)
{
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    // Initialize UART
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, 1024, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    return 0;
}

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

    /* start linkkit mqtt */
    ESP_LOGI(TAG, "Initializing LittleFS");

    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/littlefs",
        .partition_label = "storage",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };

    // Use settings defined above to initialize and mount LittleFS filesystem.
    // Note: esp_vfs_littlefs_register is an all-in-one convenience function.
    ret = esp_vfs_littlefs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find LittleFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_littlefs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get LittleFS partition information (%s)", esp_err_to_name(ret));
        esp_littlefs_format(conf.partition_label);
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

    ESP_LOGI(TAG, "UART1 initialization");
    init_uart();

    ESP_LOGI(TAG, "Start linkkit mqtt");
    linkkit_main();
}