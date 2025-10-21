# STM32F103 IAP升级系统使用说明

## 系统概述

这是一个基于UART的STM32F103在应用编程(IAP)升级系统，包含：
- Bootloader（引导程序）
- 简单的二进制传输协议
- Python上位机工具

## 内存布局

```
STM32F103C8T6 Flash Memory (64KB):
┌─────────────────────────────────────────────┐
│ 0x08000000 - 0x08003FFF (16KB)             │ ← Bootloader
├─────────────────────────────────────────────┤
│ 0x08004000 - 0x0800FFFF (48KB)             │ ← Application
└─────────────────────────────────────────────┘
```

## IAP协议格式

### 1. 开始传输
```
PC -> MCU: [0x55AA] [文件大小(4字节)]
MCU -> PC: [0xA5] (ACK) 或 [0x5A] (NACK)
```

### 2. 数据传输
```
PC -> MCU: [0x33CC] [数据包大小(2字节)] [数据内容]
MCU -> PC: [0xA5] (ACK) 或 [0x5A] (NACK)
```

### 3. 结束传输
```
PC -> MCU: [0x99FF]
MCU -> PC: [0xA5] (ACK) 或 [0x5A] (NACK)
```

## 使用步骤

### 1. 硬件连接
- USART2_TX (PA2) 连接到USB转串口的RX
- USART2_RX (PA3) 连接到USB转串口的TX
- GND连接
- 波特率：115200

### 2. 编译和烧录Bootloader
1. 在STM32CubeIDE中编译当前项目
2. 使用ST-Link将生成的.bin文件烧录到0x08000000

### 3. 准备Application固件
1. 创建应用程序项目
2. 修改链接脚本：
   ```ld
   MEMORY
   {
     FLASH (rx) : ORIGIN = 0x08004000, LENGTH = 48K
     RAM (xrw)  : ORIGIN = 0x20000000, LENGTH = 20K
   }
   ```
3. 在SystemInit中设置向量表：
   ```c
   SCB->VTOR = 0x08004000;
   ```
4. 编译生成application.bin

### 4. 使用IAP升级
1. 复位开发板
2. 在5秒内通过串口发送任意字符进入IAP模式
3. 使用Python工具上传固件：
   ```bash
   python iap_upload.py COM3 application.bin
   ```

## 启动流程

```
系统上电/复位
       ↓
   Bootloader启动
       ↓
   显示Banner信息
       ↓
   等待5秒用户输入
       ↓
┌──────────────┬─────────────────┐
↓              ↓                 ↓
用户按键      超时无输入         无有效App
↓              ↓                 ↓
IAP升级模式    检查Application    强制IAP模式
↓              ↓                 ↓
升级固件      跳转到Application   升级固件
```

## 串口调试信息

### Bootloader启动信息
```
  _____ _______ __  __ ____  ___    _____ _          _____  
 / ____|__   __|  \/  |___ \|__ \  |_   _| |   /\   |  __ \ 
| (___    | |  | \  / | __) |  ) |   | | | |  /  \  | |__) |
 \___ \   | |  | |\/| ||__ <  / /    | | | | / /\ \ |  ___/ 
 ____) |  | |  | |  | |___) |/ /_   _| |_| |/ ____ \| |     
|_____/   |_|  |_|  |_|____/|____|  |_____|_/    \_\_|     

============================================================
          STM32F103 Bootloader v1.0                       
          UART In-Application Programming                  
============================================================

Press any key to enter IAP mode...
Auto boot in 5 seconds...
Auto boot in 4 seconds...
Auto boot in 3 seconds...
Auto boot in 2 seconds...
Auto boot in 1 seconds...
Timeout! Checking application...
Jumping to application...
```

### IAP升级过程
```
Entering IAP mode...
IAP Mode: Waiting for firmware...
Received START command
File size: 12345 bytes
Erasing application area...
Ready to receive data
Progress: 10% (1234/12345 bytes)
Progress: 20% (2468/12345 bytes)
...
Progress: 100% (12345/12345 bytes)
Received END command
Total received: 12345 bytes
Firmware upgrade successful!
Restarting system...
```

## 注意事项

1. **向量表设置**：Application必须正确设置向量表偏移
2. **链接脚本**：Application的起始地址必须为0x08004000
3. **固件验证**：系统会检查栈指针和复位向量的有效性
4. **Flash擦除**：每次升级前会自动擦除Application区域
5. **超时机制**：IAP过程中如果超时会自动重启

## 故障排除

1. **无法进入IAP模式**：
   - 检查串口连接和波特率
   - 确保在5秒内发送字符

2. **升级失败**：
   - 检查固件文件大小（不超过48KB）
   - 确保Python脚本中的串口号正确
   - 检查固件的向量表设置

3. **升级后无法启动**：
   - 检查Application的链接脚本配置
   - 确认SystemInit中的向量表设置
   - 验证固件的完整性

## 扩展功能

可以添加的功能：
- CRC校验
- 固件版本管理
- 多种传输协议支持（Ymodem、Xmodem）
- 固件签名验证
- 网络升级支持
