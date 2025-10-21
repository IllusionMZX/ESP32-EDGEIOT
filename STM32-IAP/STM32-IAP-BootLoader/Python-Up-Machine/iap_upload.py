#!/usr/bin/env python3
"""
STM32F103 IAP固件上传工具
使用方法: python iap_upload.py <COM端口> <bin文件路径>
例如: python iap_upload.py COM3 application.bin
"""

import serial
import sys
import time
import struct
import os

# IAP协议定义
IAP_CMD_START = 0x55AA
IAP_CMD_DATA = 0x33CC
IAP_CMD_END = 0x99FF
IAP_ACK = 0xA5
IAP_NACK = 0x5A

def send_command(ser, cmd):
    """发送命令"""
    cmd_bytes = struct.pack('<H', cmd)
    ser.write(cmd_bytes)
    print(f"发送命令: 0x{cmd:04X}")

def wait_ack(ser, timeout=10):
    """等待应答"""
    start_time = time.time()
    received_data = []
    received_text = ""
    
    while time.time() - start_time < timeout:
        if ser.in_waiting > 0:
            data = ser.read(ser.in_waiting)
            received_data.extend(data)
            
            # 尝试解码为文本显示调试信息
            try:
                text_chunk = data.decode('utf-8', errors='ignore')
                received_text += text_chunk
                if text_chunk:
                    print(f"收到文本: {repr(text_chunk.strip())}")
            except:
                pass
            
            # 检查是否包含ACK或NACK
            for byte in data:
                if byte == IAP_ACK:
                    print("收到ACK (0xA5)")
                    return True
                elif byte == IAP_NACK:
                    print("收到NACK (0x5A)")
                    return False
        time.sleep(0.01)  # 短暂等待避免CPU占用过高
    
    print(f"等待应答超时")
    print(f"收到的文本消息: {repr(received_text)}")
    print(f"收到的原始数据: {[hex(b) for b in received_data]}")
    return False

def upload_firmware(port, bin_file):
    """上传固件"""
    try:
        # 打开串口
        ser = serial.Serial(port, 115200, timeout=1)
        print(f"已连接到 {port}")
        
        # 读取bin文件
        if not os.path.exists(bin_file):
            print(f"文件不存在: {bin_file}")
            return False
            
        with open(bin_file, 'rb') as f:
            firmware_data = f.read()
        
        file_size = len(firmware_data)
        print(f"固件文件: {bin_file}")
        print(f"文件大小: {file_size} 字节")
        
        if file_size == 0:
            print("文件为空")
            return False
        
        # 等待Bootloader启动并进入IAP模式
        print("等待Bootloader启动...")
        time.sleep(1)
        
        # 重要：清空所有缓冲区，确保同步
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        print("清空缓冲区，准备发送命令...")
        time.sleep(0.5)
        
        # 1. 发送开始命令
        print("\n--- 发送开始命令 ---")
        send_command(ser, IAP_CMD_START)
        
        if not wait_ack(ser, timeout=15):  # 等待ACK确认开始
            print("开始命令失败")
            return False
        
        # 发送文件大小
        size_bytes = struct.pack('<L', file_size)
        ser.write(size_bytes)
        print(f"发送文件大小: {file_size} 字节")
        
        if not wait_ack(ser, timeout=15):  # 等待ACK确认文件大小
            print("文件大小确认失败")
            return False
        
        # 2. 发送数据包
        print("\n--- 发送固件数据 ---")
        packet_size = 128  # 每包128字节
        total_packets = (file_size + packet_size - 1) // packet_size
        
        for i in range(total_packets):
            start_pos = i * packet_size
            end_pos = min(start_pos + packet_size, file_size)
            packet_data = firmware_data[start_pos:end_pos]
            actual_size = len(packet_data)
            
            # 发送数据命令
            send_command(ser, IAP_CMD_DATA)
            
            # 小延迟确保命令被处理
            time.sleep(0.01)
            
            # 发送数据包大小
            size_bytes = struct.pack('<H', actual_size)
            ser.write(size_bytes)
            
            # 小延迟确保大小被接收
            time.sleep(0.01)
            
            # 发送数据
            ser.write(packet_data)
            
            # 显示进度
            progress = ((i + 1) * 100) // total_packets
            print(f"进度: {progress}% ({i+1}/{total_packets}) - 发送 {actual_size} 字节")
            
            if not wait_ack(ser):
                print(f"数据包 {i+1} 发送失败")
                return False
            
            # 适当延迟
            time.sleep(0.01)
        
        # 3. 发送结束命令
        print("\n--- 发送结束命令 ---")
        send_command(ser, IAP_CMD_END)
        
        if wait_ack(ser, timeout=10):
            print("\n=== 固件上传成功! ===")
            print("设备将自动重启...")
            return True
        else:
            print("\n=== 固件上传失败! ===")
            return False
            
    except serial.SerialException as e:
        print(f"串口错误: {e}")
        return False
    except Exception as e:
        print(f"错误: {e}")
        return False
    finally:
        if 'ser' in locals():
            ser.close()

def main():
    if len(sys.argv) != 3:
        print("使用方法: python iap_upload.py <COM端口> <bin文件路径>")
        print("例如: python iap_upload.py COM3 application.bin")
        sys.exit(1)
    
    port = sys.argv[1]
    bin_file = sys.argv[2]
    
    print("=" * 50)
    print("STM32F103 IAP固件上传工具")
    print("=" * 50)
    
    success = upload_firmware(port, bin_file)
    
    if success:
        print("\n固件上传完成!")
        sys.exit(0)
    else:
        print("\n固件上传失败!")
        sys.exit(1)

if __name__ == "__main__":
    main()
