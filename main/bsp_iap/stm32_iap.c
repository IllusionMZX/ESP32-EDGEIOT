#include "stm32_iap.h"

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