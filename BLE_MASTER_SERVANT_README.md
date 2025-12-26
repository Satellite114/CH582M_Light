# BLE Master/Servant 双机通信说明

## 概述

本项目实现了两个CH582M之间的蓝牙主从通信，用于PWM控制信号传输。

## 角色定义

### Master (主机)
- **设备名称**: `CH582_MASTER`
- **功能**:
  1. 扫描并自动连接 `CH582_SERVANT`
  2. 接收来自Servant的PWM控制数据
  3. 同时支持手机连接
  4. 接收来自手机的PWM控制数据
  5. 根据接收到的数据控制PWM输出

### Servant (从机)
- **设备名称**: `CH582_SERVANT`
- **功能**:
  1. 广播自己的名称
  2. 等待Master连接
  3. 定期发送PWM控制数据给Master

## 编译配置

### 编译Master版本
在 `APP/include/ble_config.h` 中:
```c
#define BLE_ROLE_MASTER         // 启用Master模式
// #define BLE_ROLE_SERVANT     // 注释掉Servant模式
```

### 编译Servant版本
在 `APP/include/ble_config.h` 中:
```c
// #define BLE_ROLE_MASTER      // 注释掉Master模式
#define BLE_ROLE_SERVANT        // 启用Servant模式
```

## PWM控制协议

### 数据格式
2字节协议: `[total_duty, balance]`

- **total_duty** (字节0): 总占空比，范围 0-100 (%)
- **balance** (字节1): 平衡度，范围 -100 到 +100 (有符号)

### 示例
```c
uint8_t pwmData[2];
pwmData[0] = 80;   // 总占空比 80%
pwmData[1] = 20;   // 平衡度 +20

// 发送到Master
ble_uart_send_data(connHandle, pwmData, 2);
```

### PWM输出计算
```
如果 balance = 0:
  PWM1 = PWM2 = total_duty / 2

如果 balance > 0:
  PWM1 增大, PWM2 减小
  
如果 balance < 0:
  PWM1 减小, PWM2 增大

极限情况:
  balance = +100: PWM1 = total_duty, PWM2 = 0
  balance = -100: PWM1 = 0, PWM2 = total_duty
```

## 使用流程

### 1. 烧录固件
```
1. 编译Master版本，烧录到第一个CH582M
2. 编译Servant版本，烧录到第二个CH582M
```

### 2. 启动系统
```
1. 先启动Servant (从机)
   - 串口输出: "Servant mode: Advertising as CH582_SERVANT..."
   
2. 再启动Master (主机)
   - 串口输出: "Master mode: Scanning for CH582_SERVANT..."
   - 自动扫描并连接Servant
   - 连接成功后输出: "Servant connected"
```

### 3. Servant发送PWM数据
```c
// 在Servant端调用
Servant_SetPWMData(80, 20);  // 设置占空比80%, 平衡度+20
```

### 4. Master接收并应用PWM
```
Master自动接收数据并应用到PWM输出:
- PWM1 (PWM4): 根据计算结果输出
- PWM2 (PWM5): 根据计算结果输出
```

### 5. 手机连接Master
```
1. 使用BLE调试APP (如nRF Connect)
2. 扫描并连接 "CH582_MASTER"
3. 找到UART服务 (UUID: 0xFFE0)
4. 向RX特征 (UUID: 0xFFE2) 写入2字节PWM数据
   例如: [50, 00] = 占空比50%, 平衡度0
```

## 串口调试命令

### Servant端测试命令
通过串口发送命令设置PWM数据:
```
D50B10  - 设置占空比50%, 平衡度+10
D80B-20 - 设置占空比80%, 平衡度-20
D100B00 - 设置占空比100%, 平衡度0
```

## 引脚定义

### PWM输出 (Master端)
- **PWM1**: PWM4 (PB4)
- **PWM2**: PWM5 (PB6)
- **频率**: 约80kHz
- **分辨率**: 100步

### 串口调试
- **TX**: PA9
- **RX**: PA8
- **波特率**: 115200

## 连接参数

### 扫描参数 (Master)
- **扫描间隔**: 100ms
- **扫描窗口**: 50ms
- **扫描时长**: 3s

### 连接参数
- **连接间隔**: 10-25ms
- **从机延迟**: 0
- **超时时间**: 1s

### 广播参数 (Servant)
- **广播间隔**: 100ms

## 示例代码

### Master端接收处理
```c
void Master_ProcessPWMData(uint16_t connHandle, uint8_t *pData, uint8_t len)
{
    if(len == 2)
    {
        uint8_t total_duty = pData[0];
        int8_t balance = (int8_t)pData[1];
        
        printf("PWM: duty=%d%%, balance=%d\n", total_duty, balance);
        
        // 应用PWM控制
        PWM_SetDutyAndBalance(total_duty, balance);
    }
}
```

### Servant端发送
```c
void Servant_SendPWMData(void)
{
    uint8_t pwmData[2];
    pwmData[0] = currentDuty;      // 0-100
    pwmData[1] = currentBalance;   // -100 to +100
    
    ble_uart_send_data(connHandle, pwmData, 2);
}
```

## 故障排查

### Master无法找到Servant
1. 检查Servant是否正常启动并广播
2. 检查设备名称是否正确: `CH582_SERVANT`
3. 增加扫描时间或重启Master

### 连接后无数据传输
1. 检查BLE UART服务是否正确初始化
2. 检查通知是否已启用
3. 查看串口调试信息

### PWM输出异常
1. 检查接收到的数据格式是否正确
2. 检查PWM引脚配置
3. 使用示波器测量PWM输出

## 注意事项

1. **编译前务必选择正确的角色** (Master或Servant)
2. **两个设备不能同时为Master或Servant**
3. **PWM数据必须是2字节**
4. **balance是有符号数，需要正确转换**
5. **Master可以同时连接Servant和手机**
6. **Servant只能连接一个Master**

## 扩展功能

### 添加更多控制参数
修改 `PWM_DATA_LEN` 和协议定义，例如:
```c
#define PWM_DATA_LEN    4
// [total_duty, balance, frequency_high, frequency_low]
```

### 添加反馈机制
Master可以向Servant发送状态反馈:
```c
uint8_t statusData[2] = {STATUS_OK, current_state};
ble_uart_send_data(servantConnHandle, statusData, 2);
```

### 多从机支持
修改Master代码支持连接多个Servant:
```c
#define MAX_SERVANTS    3
static master_conn_t servantConn[MAX_SERVANTS];
```

---

**版本**: V1.0  
**日期**: 2025-12-26  
**作者**: Your Name

