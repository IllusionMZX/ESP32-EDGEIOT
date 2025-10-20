#include "aliyun_mqtt.h"


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