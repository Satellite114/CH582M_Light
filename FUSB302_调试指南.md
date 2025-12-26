# FUSB302 IIC通信问题调试指南

## 📋 问题描述

```
Read_Back FUSB302 chip ID :0Xff  ❌ 应该读到 0x91
Check_USB302 IS ERR
```

## 🔧 已修复的问题

### 1. GPIO引脚未配置 ✅ 已修复

**问题代码：**
```c
void FUSB302_IIC_GPIO_Init(void)
{
    // GPIOB_ModeCfg(GPIO_Pin_12 | GPIO_Pin_13, GPIO_ModeOut_PP_5mA);  ❌ 被注释了
    GPIOB_SetBits(GPIO_Pin_12 | GPIO_Pin_13);
    ...
}
```

**修复后：**
```c
void FUSB302_IIC_GPIO_Init(void)
{
    GPIOB_ModeCfg(GPIO_Pin_12 | GPIO_Pin_13, GPIO_ModeOut_PP_5mA);  ✅ 已启用
    GPIOB_SetBits(GPIO_Pin_12 | GPIO_Pin_13);
    ...
}
```

## 🧪 测试步骤

### 步骤1: 重新编译并烧录

```bash
# 清理并重新编译
make clean
make
# 烧录到芯片
```

### 步骤2: 查看串口输出

正常情况应该看到：
```
=== FUSB302 IIC通信测试 ===
Read_Back FUSB302 chip ID :0X91
✓ Check_USB302 IS OK 
  Version: 9, Revision: 1
=============================
```

### 步骤3: 如果仍然失败

#### 3.1 检查硬件连接

| 信号 | CH582引脚 | FUSB302引脚 | 说明 |
|------|-----------|-------------|------|
| SCL  | PB13      | SCL         | IIC时钟线 |
| SDA  | PB12      | SDA         | IIC数据线 |
| INT  | PB5       | INT         | 中断信号 |
| VDD  | 3.3V      | VDD         | 电源 |
| GND  | GND       | GND         | 地 |

#### 3.2 检查上拉电阻

- SCL和SDA需要 **2.2kΩ - 4.7kΩ** 上拉电阻到3.3V
- 如果没有外部上拉，可以尝试使用内部上拉（已在代码中配置）

#### 3.3 使用逻辑分析仪

抓取IIC波形，检查：
- START条件是否正确
- 地址字节：`0x44` (0x22 << 1 + Write)
- 是否收到ACK
- 寄存器地址：`0x01`
- 读取的数据

#### 3.4 尝试降低IIC速度

修改 `FUSB30X.c` 中的延时：

```c
static inline void fusb302_iic_delay(void)
{
    DelayUs(5);  // 从2us改为5us，降低速度
}
```

#### 3.5 测试IIC地址扫描

添加以下测试代码到 `main.c`：

```c
void I2C_Scan(void)
{
    printf("\n=== IIC地址扫描 ===\n");
    for (uint8_t addr = 0x08; addr < 0x78; addr++)
    {
        fusb302_iic_start();
        fusb302_iic_send_byte((addr << 1) | 0);
        if (fusb302_iic_wait_ack() == 0)
        {
            printf("发现设备: 0x%02X\n", addr);
        }
        fusb302_iic_stop();
        DelayUs(100);
    }
    printf("==================\n\n");
}

// 在main函数中调用
int main()
{
    ...
    FUSB302_IIC_GPIO_Init();
    I2C_Scan();  // 扫描IIC总线
    Check_USB302();
    ...
}
```

## 📊 常见错误代码

| 读取值 | 含义 | 可能原因 |
|--------|------|----------|
| 0xFF   | 总线无响应 | GPIO未配置、芯片未连接、上拉缺失 |
| 0x00   | SDA被拉低 | 硬件短路、芯片损坏 |
| 0x91   | ✅ 正常 | FUSB302-D Version 9 Revision 1 |
| 0x90   | ✅ 正常 | FUSB302-D Version 9 Revision 0 |
| 0x92   | ✅ 正常 | FUSB302-D Version 9 Revision 2 |

## 🔍 进阶调试

### 添加详细的IIC调试信息

在 `fusb302_iic_read_reg()` 中添加调试输出：

```c
uint8_t fusb302_iic_read_reg(uint8_t reg)
{
    uint8_t val;
    uint8_t ack1, ack2, ack3;

    fusb302_iic_start();
    fusb302_iic_send_byte((FUSB302_I2C_ADDR << 1) | FUSB302_IIC_WRITE);
    ack1 = fusb302_iic_wait_ack();
    
    fusb302_iic_send_byte(reg);
    ack2 = fusb302_iic_wait_ack();

    fusb302_iic_start(); // Repeated START
    fusb302_iic_send_byte((FUSB302_I2C_ADDR << 1) | FUSB302_IIC_READ);
    ack3 = fusb302_iic_wait_ack();

    val = fusb302_iic_recv_byte(0); // NACK for last byte
    fusb302_iic_stop();

    printf("[IIC] Reg=0x%02X ACK1=%d ACK2=%d ACK3=%d Val=0x%02X\n", 
           reg, ack1, ack2, ack3, val);

    return val;
}
```

正常情况应该看到：
```
[IIC] Reg=0x01 ACK1=0 ACK2=0 ACK3=0 Val=0x91
```

如果看到 `ACK1=1`，说明芯片没有响应地址。

## 📝 检查清单

- [x] GPIO引脚已正确配置为输出模式
- [ ] 硬件连接正确（SCL=PB13, SDA=PB12）
- [ ] 上拉电阻存在（2.2kΩ - 4.7kΩ）
- [ ] FUSB302供电正常（3.3V）
- [ ] IIC地址正确（0x22）
- [ ] 时序延时足够（2us）
- [ ] 芯片未损坏

## 🎯 预期结果

修复后，您应该看到：

```
FUSB302 配置软件IIC完成
  SCL: PB13
  SDA: PB12
  INT: PB5

=== FUSB302 IIC通信测试 ===
Read_Back FUSB302 chip ID :0X91
✓ Check_USB302 IS OK 
  Version: 9, Revision: 1
=============================

Checking PD UFP..
connected ，未读取到FUSB302的ID，检查现在的软件IIC配置是否存在问题
```

## 📞 如果问题仍未解决

请提供以下信息：
1. 完整的串口输出日志
2. 硬件原理图（IIC部分）
3. 逻辑分析仪抓取的IIC波形（如有）
4. 使用的FUSB302芯片型号
5. 上拉电阻阻值

---

**最后更新：** 2025-12-26
**修复版本：** v1.1

