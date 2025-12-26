# FUSB302 函数移植问题修复说明

## 问题分析

您提到的三个函数在移植过程中确实存在问题：

### 1. USB302_Wite_Reg() - ✅ 基本正确
```c
USB302_Wite_Reg(0x06, 0x40); // 清发送
```
这个函数实现基本正确，没有明显问题。

### 2. USB302_Wite_FIFO() - ❌ **主要问题**
```c
USB302_Wite_FIFO(USB302_TX_Buff, 14);
```

**原始代码问题：**
```c
void USB302_Wite_FIFO(uint8_t *data, uint8_t length)
{
    const uint8_t FIFO_ADDR = 0x43;
    uint8_t buf[length + 1];  // ⚠️ 变长数组（VLA）
    ...
}
```

**问题点：**
- **变长数组（VLA）**：在嵌入式环境中可能导致栈溢出
- **内存对齐问题**：某些编译器可能不正确处理VLA
- **栈空间不确定**：每次调用占用的栈空间不同

**修复方案：**
```c
void USB302_Wite_FIFO(uint8_t *data, uint8_t length)
{
    const uint8_t FIFO_ADDR = 0x43;
    uint8_t buf[64];  // 固定大小缓冲区
    
    if (length > 63) {
        printf("错误: FIFO写入长度超限\n");
        return;
    }
    ...
}
```

### 3. USB302_Wite_Reg(0x06, 0x05) - ⚠️ 时序问题
```c
USB302_Wite_Reg(0x06, 0x05);  // 开始发送
```

**问题点：**
- 三个操作之间缺少足够的延时
- I2C总线可能还没完成前一个操作

**修复方案：**
```c
// 步骤1: 清空发送FIFO
USB302_Wite_Reg(0x06, 0x40);
DelayUs(100);  // 等待FIFO清空

// 步骤2: 写入数据到FIFO
USB302_Wite_FIFO(USB302_TX_Buff, 14);
DelayUs(100);  // 等待数据写入

// 步骤3: 启动发送
USB302_Wite_Reg(0x06, 0x05);
```

## 其他发现的问题

### 4. i2c_transfer_data() - 缺少错误处理
**原始代码：**
```c
void i2c_transfer_data(uint8_t addr, uint8_t data_len, uint8_t *data)
{
    while (I2C_GetFlagStatus(I2C_FLAG_BUSY) != RESET);  // ⚠️ 可能死循环
    ...
}
```

**修复：**
- 添加超时保护
- 添加错误检测
- 添加调试信息

### 5. I2C数据发送顺序问题
**原始代码：**
```c
for (i = 0; i < data_len; i++)
{
    while (I2C_GetFlagStatus(I2C_FLAG_TXE) == RESET);  // 先等待
    I2C_SendData(data[i]);  // 后发送
}
```

**修复：**
```c
for (i = 0; i < data_len; i++)
{
    I2C_SendData(data[i]);  // 先发送
    while (I2C_GetFlagStatus(I2C_FLAG_TXE) == RESET);  // 后等待
}
```

## 修改总结

### 已修复的文件
- `FUSB_PD/FUSB30X.c`

### 主要修改内容

1. **USB302_Wite_FIFO()**
   - 将变长数组改为固定大小数组（64字节）
   - 添加长度检查
   - 添加错误提示

2. **USB302_Send_Requse()**
   - 增加操作间延时（10us → 100us）
   - 添加调试信息输出
   - 添加状态寄存器读取
   - 打印发送缓冲区内容

3. **i2c_transfer_data()**
   - 添加超时保护（5000次循环）
   - 添加错误检测和提示
   - 添加STOP后延时
   - 修正数据发送顺序

4. **新增测试函数**
   - `USB302_Test_FIFO_Write()` - 用于测试FIFO写入功能

## 测试建议

### 1. 基础测试
```c
// 在main函数中调用
Check_USB302();  // 检查芯片ID
USB302_Test_FIFO_Write();  // 测试FIFO写入
```

### 2. 观察调试输出
运行后应该看到：
```
发送PD请求: 档位X, MSG_ID=X
TX_Buff: 12 12 12 13 86 42 14 00 00 00 03 FF 14 A1
清空FIFO...
写入FIFO...
启动发送...
发送后状态寄存器0x41: 0xXX
PD请求已发送，等待响应...
```

### 3. 检查点
- [ ] 芯片ID读取正常（0x91）
- [ ] I2C通信无超时错误
- [ ] FIFO写入成功
- [ ] 能收到PD响应（Accept/PS_RDY）

## 可能还需要检查的地方

### 1. I2C时钟频率
FUSB302要求I2C时钟频率：
- 标准模式：100kHz
- 快速模式：400kHz

**检查您的I2C初始化代码**

### 2. I2C上拉电阻
确保SCL和SDA有合适的上拉电阻（通常2.2kΩ - 4.7kΩ）

### 3. 寄存器0x06的值
- `0x40` - 清空TX FIFO
- `0x05` - 启动发送（bit0=1启动TX, bit2=1启动RX）

可能需要改为：
```c
USB302_Wite_Reg(0x06, 0x01);  // 只启动TX，不启动RX
```

### 4. 检查PD协议版本
确认您的代码中PD版本设置正确：
```c
uint8_t PD_Version = 2;  // PD 2.0
```

## 如果问题仍然存在

### 调试步骤：

1. **使用逻辑分析仪**
   - 抓取I2C波形
   - 检查START/STOP条件
   - 检查ACK/NACK

2. **读取FUSB302状态寄存器**
   ```c
   uint8_t status0 = USB302_Read_Reg(0x40);
   uint8_t status1 = USB302_Read_Reg(0x41);
   uint8_t interrupt = USB302_Read_Reg(0x42);
   printf("Status0=0x%02X, Status1=0x%02X, Int=0x%02X\n", 
          status0, status1, interrupt);
   ```

3. **检查中断标志**
   - TX_SUCCESS (0x42 bit6)
   - TX_FAILED (0x42 bit5)
   - COLLISION (0x42 bit4)

4. **尝试降低I2C速度**
   如果问题仍存在，尝试将I2C时钟降低到100kHz

## 联系方式
如果问题仍未解决，请提供：
1. 逻辑分析仪抓取的I2C波形
2. 完整的调试输出
3. FUSB302状态寄存器的值

