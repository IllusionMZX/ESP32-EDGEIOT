# ESP32-STM32 Edge IoT Platform

This project integrates multiple technologies to create a IoT platform using ESP32 and STM32. The platform supports firmware upgrades, file storage, USB communication, and Bluetooth functionalities.

[English](#esp32-stm32-edge-iot-platform) | [中文](#esp32-stm32-边缘物联网平台)

## Features

### 1. **Aliyun MQTT Integration**
- Integrated with Aliyun MQTT platform.
- Enables downloading ESP32 firmware upgrade packages.
- Facilitates UART-based IAP (In-Application Programming) upgrades for STM32.

### 2. **LittleFS File System**
- Integrated LittleFS for file storage.
- Stores upgrade files and other data.
- Includes a file server to view and manage files stored in the LittleFS file system.

### 3. **TinyUSB Integration**
- Integrated TinyUSB for USB communication.
- Supports USB-based data exchange and interaction.

### 4. **NimBLE Bluetooth Protocol Stack**
- Integrated NimBLE for Bluetooth communication.
- Implements GATT server for Bluetooth functionalities.
- Enables Bluetooth-based control of lighting and other features.

## Dependencies

The project relies on the following components:

- **Aliyun MQTT**: For MQTT communication with the Aliyun platform.
- **LittleFS**: For lightweight file storage.
- **TinyUSB**: For USB communication.
- **NimBLE**: For Bluetooth communication and GATT server implementation.
- **LED Strip**: For controlling lighting via Bluetooth.

### Component Versions
The `idf_component.yml` specifies the following dependencies:
```yaml
dependencies:
  joltwallet/littlefs: "~=1.20.0"
  espressif/esp_tinyusb: "^1"
  espressif/led_strip: "^2.4.1"
```

## Directory Structure

- **`main/`**: Contains the main application logic, including BSP (Board Support Package) for Aliyun MQTT, Bluetooth, IAP, LittleFS, UART, USB, and WiFi server.
- **`components/aliyun_mqtt/`**: Aliyun MQTT component for cloud communication.
- **`flash_data/`**: Stores example files for testing.
- **`LVGL-Simulator/`**: Contains resources for LVGL simulation.
- **`STM32-IAP/`**: Includes STM32 bootloader and FreeRTOS-based IAP implementation.

## Partition Table

The project uses the following partition table for the ESP32S3 chip with 16MB flash:

| Name      | Type  | SubType  | Offset   | Size  |
|-----------|-------|----------|----------|-------|
| nvs       | data  | nvs      | 0x9000   | 24KB  |
| phy_init  | data  | phy      | 0xf000   | 4KB   |
| factory   | app   | factory  | 0x10000  | 10MB  |
| storage   | data  | littlefs |          | 5MB   |

The STM32 uses the STM32F103C8T6 chip.


## Getting Started

1. Clone the repository.
2. Configure the project using `idf.py menuconfig`.
3. Build and flash the firmware using `idf.py build` and `idf.py flash`.
4. Connect to the Aliyun MQTT platform and test the functionalities.


# ESP32-STM32 边缘物联网平台

该项目集成了多种技术，创建了一个基于 ESP32 和 STM32 的物联网平台。平台支持固件升级、文件存储、USB 通信和蓝牙功能。

[English](#esp32-stm32-edge-iot-platform) | [中文](#esp32-stm32-边缘物联网平台)

## 功能特点

### 1. **阿里云 MQTT 集成**
- 集成阿里云 MQTT 平台。
- 支持下载 ESP32 固件升级包。
- 实现 STM32 的基于 UART 的 IAP（应用内编程）升级。

### 2. **LittleFS 文件系统**
- 集成 LittleFS 文件系统，用于文件存储。
- 存储升级文件和其他数据。
- 包含文件服务器，用于查看和管理存储在 LittleFS 文件系统中的文件。

### 3. **TinyUSB 集成**
- 集成 TinyUSB，用于 USB 通信。
- 支持基于 USB 的数据交换和交互。

### 4. **NimBLE 蓝牙协议栈**
- 集成 NimBLE，用于蓝牙通信。
- 实现 GATT 服务器功能。
- 支持基于蓝牙的灯光控制和其他功能。

## 依赖项

该项目依赖以下组件：

- **阿里云 MQTT**：用于与阿里云平台的 MQTT 通信。
- **LittleFS**：用于轻量级文件存储。
- **TinyUSB**：用于 USB 通信。
- **NimBLE**：用于蓝牙通信和 GATT 服务器实现。
- **LED Strip**：用于通过蓝牙控制灯光。

### 组件版本
`idf_component.yml` 中指定了以下依赖项：
```yaml
dependencies:
  joltwallet/littlefs: "~=1.20.0"
  espressif/esp_tinyusb: "^1"
  espressif/led_strip: "^2.4.1"
```

## 目录结构

- **`main/`**：包含主要应用逻辑，包括阿里云 MQTT、蓝牙、IAP、LittleFS、UART、USB 和 WiFi 服务器的 BSP（板级支持包）。
- **`components/aliyun_mqtt/`**：阿里云 MQTT 组件，用于云通信。
- **`flash_data/`**：存储测试用的示例文件。
- **`LVGL-Simulator/`**：包含 LVGL 仿真资源。
- **`STM32-IAP/`**：包括 STM32 引导加载程序和基于 FreeRTOS 的 IAP 实现。

## 分区表

该项目为具有 16MB 闪存的 ESP32S3 芯片使用以下分区表：

| 名称      | 类型  | 子类型  | 偏移量   | 大小  |
|-----------|-------|----------|----------|-------|
| nvs       | data  | nvs      | 0x9000   | 24KB  |
| phy_init  | data  | phy      | 0xf000   | 4KB   |
| factory   | app   | factory  | 0x10000  | 10MB  |
| storage   | data  | littlefs |          | 5MB   |

STM32 使用 STM32F103C8T6 芯片。


## 快速开始

1. 克隆仓库。
2. 使用 `idf.py menuconfig` 配置项目。
3. 使用 `idf.py build` 和 `idf.py flash` 构建并烧录固件。
4. 连接到阿里云 MQTT 平台并测试功能。