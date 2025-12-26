# 蓝牙PWM控制系统使用说明

## 功能概述

本系统通过蓝牙串口接收2字节数据来控制PWM4（PA12）和PWM5（PA13）的输出占空比。

## 硬件连接

- **PWM4输出**: PA12
- **PWM5输出**: PA13
- **PWM频率**: 约78kHz
- **分辨率**: 256级（0-100%）

## 蓝牙通信协议

### 数据格式

每次发送 **2个字节** 的数据：

```
[字节0] [字节1]
```

- **字节0**: 总占空比 (0-100)
  - 表示PWM4和PWM5的占空比之和
  - 范围：0-100（对应0%-100%）

- **字节1**: PWM4占总占空比的百分比 (0-100)
  - 表示PWM4在总占空比中所占的比例
  - 范围：0-100（对应0%-100%）

### 计算公式

```
PWM4占空比 = 总占空比 × PWM4百分比 / 100
PWM5占空比 = 总占空比 - PWM4占空比
```

## 使用示例

### 示例1：总占空比50%，PWM4占30%

**发送数据**: `0x32 0x1E` (十进制: 50, 30)

**计算结果**:
- 总占空比 = 50%
- PWM4百分比 = 30%
- PWM4占空比 = 50% × 30% = 15%
- PWM5占空比 = 50% - 15% = 35%

**输出**:
- PA12 (PWM4): 15% 占空比
- PA13 (PWM5): 35% 占空比

### 示例2：总占空比80%，PWM4占60%

**发送数据**: `0x50 0x3C` (十进制: 80, 60)

**计算结果**:
- 总占空比 = 80%
- PWM4百分比 = 60%
- PWM4占空比 = 80% × 60% = 48%
- PWM5占空比 = 80% - 48% = 32%

**输出**:
- PA12 (PWM4): 48% 占空比
- PA13 (PWM5): 32% 占空比

### 示例3：总占空比100%，PWM4占100%（仅PWM4输出）

**发送数据**: `0x64 0x64` (十进制: 100, 100)

**计算结果**:
- 总占空比 = 100%
- PWM4百分比 = 100%
- PWM4占空比 = 100% × 100% = 100%
- PWM5占空比 = 100% - 100% = 0%

**输出**:
- PA12 (PWM4): 100% 占空比
- PA13 (PWM5): 0% 占空比（关闭）

### 示例4：总占空比100%，PWM4占0%（仅PWM5输出）

**发送数据**: `0x64 0x00` (十进制: 100, 0)

**计算结果**:
- 总占空比 = 100%
- PWM4百分比 = 0%
- PWM4占空比 = 100% × 0% = 0%
- PWM5占空比 = 100% - 0% = 100%

**输出**:
- PA12 (PWM4): 0% 占空比（关闭）
- PA13 (PWM5): 100% 占空比

### 示例5：关闭所有PWM输出

**发送数据**: `0x00 0x00` (十进制: 0, 0)

**计算结果**:
- 总占空比 = 0%
- PWM4占空比 = 0%
- PWM5占空比 = 0%

**输出**:
- PA12 (PWM4): 0% 占空比（关闭）
- PA13 (PWM5): 0% 占空比（关闭）

## 蓝牙连接步骤

1. **编译并烧录程序**
   - 使用 `main_ble_pwm.c` 作为主程序
   - 编译并下载到CH582芯片

2. **搜索蓝牙设备**
   - 设备名称: `ch583_ble_uart`
   - 使用手机或电脑的蓝牙搜索功能

3. **连接设备**
   - 配对密码: `000000`（如果需要）

4. **打开串口助手**
   - 使用支持BLE串口的APP（如：Serial Bluetooth Terminal、nRF Connect等）
   - 连接到设备的串口服务

5. **发送控制命令**
   - 以十六进制格式发送2字节数据
   - 例如：`32 1E` 表示总占空比50%，PWM4占30%

## 手机APP推荐

### Android
- **Serial Bluetooth Terminal** (推荐)
- **nRF Connect**
- **BLE Scanner**

### iOS
- **LightBlue**
- **nRF Connect**
- **BLE Terminal**

## 串口调试信息

通过UART1（PA9-TXD1）可以查看调试信息：

```
BLE PWM Control System Started
Waiting for BLE connection...
BLE UART TX notification enabled
BLE Data Received: len=2
[BLE PWM] Total=50%, PWM4_ratio=30%
[BLE PWM] PWM4=15%, PWM5=35%
[BLE PWM] Calculated balance=-40
```

## Python测试脚本示例

```python
import serial
import time

# 连接蓝牙串口（需要先配对）
# Windows: 'COM3', Linux: '/dev/rfcomm0', Mac: '/dev/tty.HC-05-DevB'
ser = serial.Serial('COM3', 9600, timeout=1)

def set_pwm(total_duty, pwm4_ratio):
    """
    设置PWM输出
    total_duty: 总占空比 (0-100)
    pwm4_ratio: PWM4占总占空比的百分比 (0-100)
    """
    # 限制范围
    total_duty = max(0, min(100, total_duty))
    pwm4_ratio = max(0, min(100, pwm4_ratio))
    
    # 发送2字节数据
    data = bytes([total_duty, pwm4_ratio])
    ser.write(data)
    print(f"发送: 总占空比={total_duty}%, PWM4比例={pwm4_ratio}%")
    
    # 计算实际占空比
    pwm4_duty = total_duty * pwm4_ratio // 100
    pwm5_duty = total_duty - pwm4_duty
    print(f"PWM4={pwm4_duty}%, PWM5={pwm5_duty}%")

# 测试示例
print("=== 蓝牙PWM控制测试 ===\n")

# 示例1: 总50%, PWM4占30%
set_pwm(50, 30)
time.sleep(2)

# 示例2: 总80%, PWM4占60%
set_pwm(80, 60)
time.sleep(2)

# 示例3: 总100%, PWM4占100%
set_pwm(100, 100)
time.sleep(2)

# 示例4: 总100%, PWM4占0%
set_pwm(100, 0)
time.sleep(2)

# 示例5: 关闭
set_pwm(0, 0)

ser.close()
```

## 注意事项

1. **数据范围**: 两个字节的值都必须在0-100之间，超出范围会被自动限制
2. **数据长度**: 必须发送完整的2字节数据，否则会被忽略
3. **连接状态**: 只有在蓝牙连接并启用通知后才能接收数据
4. **PWM频率**: 固定为约78kHz，不可调整
5. **引脚配置**: PWM4和PWM5分别固定在PA12和PA13

## 故障排除

### 问题1: 无法搜索到蓝牙设备
- 检查程序是否正确烧录
- 检查芯片是否正常供电
- 重启设备

### 问题2: 发送数据后PWM无变化
- 检查是否已启用通知（Notification）
- 确认发送的数据格式正确（2字节）
- 查看串口调试信息

### 问题3: PWM输出不正确
- 使用示波器检查PA12和PA13的波形
- 确认发送的数据值在有效范围内
- 检查硬件连接

## 技术支持

如有问题，请查看串口调试输出或联系技术支持。

---

**版本**: V1.0  
**日期**: 2025/12/26  
**作者**: WCH

