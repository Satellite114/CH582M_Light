#include "FUSB30X.h"
#include "string.h"

uint8_t CCx_PIN_Useful = 0; // 0为没有 1为cc1 2为cc2 注意 为1的时候 不排除2有效
uint8_t USB302_TX_Buff[20];
uint8_t USB302_RX_Buff[40];
uint8_t RX_Length = 0;
uint8_t PD_STEP = 0;

uint8_t PD_MSG_ID = 0;
uint8_t PD_Version = 2;
PD_Source_Capabilities_TypeDef PD_Source_Capabilities_Inf[7];
uint8_t PD_Source_Capabilities_Inf_num = 0;

/**
 * @brief       IIC接口延时函数，用于控制IIC读写速度
 * @param       无
 * @retval      无
 */
static inline void fusb302_iic_delay(void)
{
    DelayUs(2);
}

/**
 * @brief       产生IIC起始信号
 * @param       无
 * @retval      无
 */
static void fusb302_iic_start(void)
{
    FUSB302_IIC_SDA(1);
    FUSB302_IIC_SCL(1);
    fusb302_iic_delay();
    FUSB302_IIC_SDA(0);
    fusb302_iic_delay();
    FUSB302_IIC_SCL(0);
    fusb302_iic_delay();
}

/**
 * @brief       产生IIC停止信号
 * @param       无
 * @retval      无
 */
static void fusb302_iic_stop(void)
{
    FUSB302_IIC_SDA(0);
    fusb302_iic_delay();
    FUSB302_IIC_SCL(1);
    fusb302_iic_delay();
    FUSB302_IIC_SDA(1);
    fusb302_iic_delay();
}

/**
 * @brief       等待IIC应答信号
 * @param       无
 * @retval      0: 应答信号接收成功
 *              1: 应答信号接收失败
 */
static uint8_t fusb302_iic_wait_ack(void)
{
    uint8_t waittime = 0;
    uint8_t rack = 0;

    FUSB302_IIC_SDA(1);
    fusb302_iic_delay();
    FUSB302_IIC_SCL(1);
    fusb302_iic_delay();

    while (FUSB302_IIC_READ_SDA())
    {
        waittime++;

        if (waittime > 250)
        {
            fusb302_iic_stop();
            rack = 1;
            break;
        }
    }

    FUSB302_IIC_SCL(0);
    fusb302_iic_delay();

    return rack;
}

/**
 * @brief       产生ACK应答信号
 * @param       无
 * @retval      无
 */
static void fusb302_iic_ack(void)
{
    FUSB302_IIC_SDA(0);
    fusb302_iic_delay();
    FUSB302_IIC_SCL(1);
    fusb302_iic_delay();
    FUSB302_IIC_SCL(0);
    fusb302_iic_delay();
    FUSB302_IIC_SDA(1);
    fusb302_iic_delay();
}

/**
 * @brief       不产生ACK应答信号
 * @param       无
 * @retval      无
 */
static void fusb302_iic_nack(void)
{
    FUSB302_IIC_SDA(1);
    fusb302_iic_delay();
    FUSB302_IIC_SCL(1);
    fusb302_iic_delay();
    FUSB302_IIC_SCL(0);
    fusb302_iic_delay();
}

/**
 * @brief       IIC发送一个字节
 * @param       dat: 要发送的数据
 * @retval      无
 */
static void fusb302_iic_send_byte(uint8_t dat)
{
    uint8_t t;

    for (t = 0; t < 8; t++)
    {
        FUSB302_IIC_SDA((dat & 0x80) >> 7);
        fusb302_iic_delay();
        FUSB302_IIC_SCL(1);
        fusb302_iic_delay();
        FUSB302_IIC_SCL(0);
        dat <<= 1;
    }
    FUSB302_IIC_SDA(1);
}

/**
 * @brief       IIC接收一个字节
 * @param       ack: ack=1时，发送ack; ack=0时，发送nack
 * @retval      接收到的数据
 */
static uint8_t fusb302_iic_recv_byte(uint8_t ack)
{
    uint8_t i;
    uint8_t dat = 0;

    for (i = 0; i < 8; i++)
    {
        dat <<= 1;
        FUSB302_IIC_SCL(1);
        fusb302_iic_delay();

        if (FUSB302_IIC_READ_SDA())
        {
            dat++;
        }

        FUSB302_IIC_SCL(0);
        fusb302_iic_delay();
    }

    if (ack == 0)
    {
        fusb302_iic_nack();
    }
    else
    {
        fusb302_iic_ack();
    }

    return dat;
}

/**
 * @brief       初始化IIC接口
 * @param       无
 * @retval      无
 */
void fusb302_iic_init(void)
{
    fusb302_iic_stop();
}

/**
 * @brief   解析PD档位信息
 * @param   index: 档位索引(0~6)
 * @param   voltage_mv: 输出电压(mV)
 * @param   current_ma: 输出电流(mA)
 * @return  0=成功, 1=无效索引
 */
uint8_t USB302_Parse_PDO(uint8_t index, uint16_t *voltage_mv, uint16_t *current_ma)
{
    if (index >= PD_Source_Capabilities_Inf_num || index >= 7)
    {
        return 1; // 无效索引
    }

    uint8_t *pdc = PD_Source_Capabilities_Inf[index].PDC_INF;

    /* 检查是否为FIXED电源类型 (bit30-31 == 0) */
    if ((pdc[3] & 0xC0) != 0)
    {
        return 1; // 非FIXED类型
    }

    /* 解析电压 (50mV单位) */
    uint16_t vol = ((pdc[2] & 0x0F) << 8) | (pdc[1] & 0xFC);
    vol >>= 2;
    *voltage_mv = vol * 50;

    /* 解析电流 (10mA单位) */
    uint16_t cur = ((pdc[1] & 0x03) << 8) | pdc[0];
    *current_ma = cur * 10;

    return 0;
}

const uint8_t PD_Resq[14] =
    {
        TokenTx_SOP1, TokenTx_SOP1, TokenTx_SOP1, TokenTx_SOP2, TokenTx_PACKSYM + 6,
        0x42, 0x14,
        0x00, 0x00, 0x00, 0x03,
        0xff, 0x14, 0xA1};

/**
 * @brief       读FUSB302寄存器
 * @param       reg: 寄存器地址
 * @retval      读取到的寄存器值
 */
uint8_t fusb302_iic_read_reg(uint8_t reg)
{
    uint8_t val;

    fusb302_iic_start();
    fusb302_iic_send_byte((FUSB302_I2C_ADDR << 1) | FUSB302_IIC_WRITE);
    fusb302_iic_wait_ack();
    fusb302_iic_send_byte(reg);
    fusb302_iic_wait_ack();

    fusb302_iic_start(); // Repeated START
    fusb302_iic_send_byte((FUSB302_I2C_ADDR << 1) | FUSB302_IIC_READ);
    fusb302_iic_wait_ack();

    val = fusb302_iic_recv_byte(0); // NACK for last byte

    fusb302_iic_stop();

    return val;
}

/* 兼容旧函数名 */
uint8_t USB302_Read_Reg(uint8_t REG_ADDR)
{
    return fusb302_iic_read_reg(REG_ADDR);
}

/**
 * @brief       写FUSB302寄存器
 * @param       reg: 寄存器地址
 * @param       val: 要写入的值
 * @retval      0: 成功, 1: 失败
 */
uint8_t fusb302_iic_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t ret;

    fusb302_iic_start();
    fusb302_iic_send_byte((FUSB302_I2C_ADDR << 1) | FUSB302_IIC_WRITE);
    fusb302_iic_wait_ack();
    fusb302_iic_send_byte(reg);
    fusb302_iic_wait_ack();
    fusb302_iic_send_byte(val);
    ret = fusb302_iic_wait_ack();
    fusb302_iic_stop();

    return ret;
}

/* 兼容旧函数名 */
void USB302_Wite_Reg(uint8_t addr, uint8_t val)
{
    fusb302_iic_write_reg(addr, val);
}

/**
 * @brief       读FUSB302 FIFO
 * @param       pBuf: 接收缓冲区
 * @param       len: 读取长度
 * @retval      无
 */
void fusb302_iic_read_fifo(uint8_t *pBuf, uint8_t len)
{
    uint8_t buf_index;
    const uint8_t FIFO_ADDR = 0x43;

    fusb302_iic_start();
    fusb302_iic_send_byte((FUSB302_I2C_ADDR << 1) | FUSB302_IIC_WRITE);
    fusb302_iic_wait_ack();
    fusb302_iic_send_byte(FIFO_ADDR);
    fusb302_iic_wait_ack();

    fusb302_iic_start(); // Repeated START
    fusb302_iic_send_byte((FUSB302_I2C_ADDR << 1) | FUSB302_IIC_READ);
    fusb302_iic_wait_ack();

    for (buf_index = 0; buf_index < len - 1; buf_index++)
    {
        pBuf[buf_index] = fusb302_iic_recv_byte(1); // ACK
    }

    pBuf[buf_index] = fusb302_iic_recv_byte(0); // NACK for last byte

    fusb302_iic_stop();
}

/* 兼容旧函数名 */
void USB302_Read_FIFO(uint8_t *pBuf, uint8_t len)
{
    fusb302_iic_read_fifo(pBuf, len);
}

/**
 * @brief       写FUSB302 FIFO
 * @param       data: 要写入的数据
 * @param       length: 数据长度
 * @retval      无
 */
void fusb302_iic_write_fifo(uint8_t *data, uint8_t length)
{
    uint8_t i;
    const uint8_t FIFO_ADDR = 0x43;

    if (length == 0)
        return;

    fusb302_iic_start();
    fusb302_iic_send_byte((FUSB302_I2C_ADDR << 1) | FUSB302_IIC_WRITE);
    fusb302_iic_wait_ack();
    fusb302_iic_send_byte(FIFO_ADDR);
    fusb302_iic_wait_ack();

    for (i = 0; i < length; i++)
    {
        fusb302_iic_send_byte(data[i]);
        fusb302_iic_wait_ack();
    }

    fusb302_iic_stop();
}

/* 兼容旧函数名 */
void USB302_Wite_FIFO(uint8_t *data, uint8_t length)
{
    fusb302_iic_write_fifo(data, length);
}

void Check_USB302(void)
{
    uint8_t Read_Back;

    printf("\n=== FUSB302 IIC Communication Test ===\n");

    // Test 1: Read Device ID register (0x01)
    Read_Back = USB302_Read_Reg(0x01);
    printf("Read_Back FUSB302 chip ID :0X%x\n", Read_Back);

    if (Read_Back == 0x91 || Read_Back == 0x90 || Read_Back == 0x92)
    {
        printf("Check_USB302 IS OK \n");
        printf("  Version: %d, Revision: %d\n", (Read_Back >> 4) & 0x0F, Read_Back & 0x0F);
    }
    else if (Read_Back == 0xFF)
    {
        printf("Check_USB302 IS ERR - Read 0xFF\n");
        printf("  Possible reasons:\n");
        printf("  1. IIC pins not configured\n");
        printf("  2. FUSB302 not connected or damaged\n");
        printf("  3. IIC address error (current:0x%02X)\n", FUSB302_I2C_ADDR);
        printf("  4. Pull-up resistor missing\n");
    }
    else if (Read_Back == 0x00)
    {
        printf("Check_USB302 IS ERR - Read 0x00\n");
        printf("  Possible reason: SDA pulled low, check hardware\n");
    }
    else
    {
        printf("Check_USB302 IS ERR - Unknown ID: 0x%02X\n", Read_Back);
    }

    printf("=======================================\n\n");
}

// 检测cc脚上是否有连接
// 返回 0 失败， 1 成功
uint8_t USB302_Chech_CCx(void)
{
    uint8_t Read_State;
    USB302_Wite_Reg(0x0C, 0x02); // PD Reset
    USB302_Wite_Reg(0x0C, 0x03); // Reset FUSB302
    DelayMs(5);
    USB302_Wite_Reg(0x0B, 0x0F); // FULL POWER!
    USB302_Wite_Reg(0x02, 0x07); // Switch on MEAS_CC1
    DelayMs(2);
    Read_State = USB302_Read_Reg(0x40); // 读状态
    USB302_Wite_Reg(0x02, 0x03);        // 切换到初始状态
    Read_State &= 0x03;                 // 只看低2位 看主机有没有电压
    if (Read_State > 0)
    {
        CCx_PIN_Useful = 1;
        return 1;
    }
    USB302_Wite_Reg(0x02, 0x0B); // Switch on MEAS_CC2
    DelayMs(2);
    Read_State = USB302_Read_Reg(0x40); // 读状态
    USB302_Wite_Reg(0x02, 0x03);        // 切换到初始状态
    Read_State &= 0x03;                 // 只看低2位 看主机有没有电压

    if (Read_State > 0)
    {
        CCx_PIN_Useful = 2;
        return 1;
    }
    return 0;
}

// 返回 0 失败， 1 成功
uint8_t USB302_Init(void)
{
    printf("Checking PD UFP..\n");
    if (USB302_Chech_CCx() == 0)
        return 0;                // 检查有没有接着设备
    USB302_Wite_Reg(0x09, 0x40); // 发送硬件复位包
    USB302_Wite_Reg(0x0C, 0x03); // Reset FUSB302
    DelayMs(5);
    USB302_Wite_Reg(0x09, 0x07); // 使能自动重试 3次自动重试
    USB302_Wite_Reg(0x0E, 0xFC); // 使能各种中断
    // USB302_Wite_Reg(0x0F, 0xFF);
    USB302_Wite_Reg(0x0F, 0x01);
    USB302_Wite_Reg(0x0A, 0xEF);
    USB302_Wite_Reg(0x06, 0x00); // 清空各种状态
    USB302_Wite_Reg(0x0C, 0x02); // 复位PD
    if (CCx_PIN_Useful == 1)
    {
        // USB302_Wite_Reg(0x02, 0x07); // Switch on MEAS_CC1
        USB302_Wite_Reg(0x02, 0x05); // Switch on MEAS_CC1
        USB302_Wite_Reg(0x03, 0x41); // Enable BMC Tx on_CC1 PD3.0
                                     // USB302_Wite_Reg(0x03, 0x45); // Enable BMC Tx on_CC1 PD3.0 AutoCRC
    }
    else if (CCx_PIN_Useful == 2)
    {
        // USB302_Wite_Reg(0x02, 0x0B); // Switch on MEAS_CC2
        USB302_Wite_Reg(0x02, 0x0A); // Switch on MEAS_CC2
        USB302_Wite_Reg(0x03, 0x42); // Enable BMC Tx on CC2 PD3.0
                                     // USB302_Wite_Reg(0x03, 0x46); // Enable BMC Tx on_CC1 PD3.0 AutoCRC
    }
    USB302_Wite_Reg(0x0B, 0x0F); // 全电源
    USB302_Read_Reg(0x3E);
    USB302_Read_Reg(0x3F);
    USB302_Read_Reg(0x42);
    RX_Length = 0;
    PD_STEP = 0;
    PD_Source_Capabilities_Inf_num = 0;
    /*  USB302_Wite_Reg(0x07, 0x04); // Flush RX*/
    return 1;
}

void FUSB30XRefreshStatusRegister(void)
{
    USB302_Read_Reg(0x3E);
    USB302_Read_Reg(0x3F);
    USB302_Read_Reg(0x42); // 清中断
}
void USB302_Read_Service(void) // 读取服务
{
    uint8_t readSize;
    FUSB30XRefreshStatusRegister(); // 清中断
    USB302_RX_Buff[0] = USB302_Read_Reg(0x43) & 0xe0;
    if (USB302_RX_Buff[0] > 0x40)                   // E0 C0 A0 80 60 都是允许的值
    {                                               // 小端 高8位后来
        USB302_Read_FIFO(USB302_RX_Buff + 1, 2);    // read header
        readSize = USB302_RX_Buff[2] & 0x70;        // 取数量位 报告了有几组电压的意思
        readSize = ((readSize >> 4) & 0x7) * 4 + 4; // 每个电压报告组有4字节 32bit
        RX_Length = readSize + 3;
        USB302_Read_FIFO(USB302_RX_Buff + 3, readSize);
    }
    else
        RX_Length = 0;
    USB302_Wite_Reg(0x07, 0x04); // 清空RX FIFO
}

void PD_Msg_ID_ADD(void) // 成功通讯多少次，也就是收到goodcrc的次数，最大为7
{
    PD_MSG_ID++;
    if (PD_MSG_ID > 7)
        PD_MSG_ID = 0;
}
void USB302_Data_Service(void) // 数据服务
{
    uint8_t i;
    uint8_t j = 0;
    if (READ_FUSB30X_INT == 0)
    {
        USB302_Read_Service();
        if (RX_Length >= 5) // 至少要读得5个包
        {
            PD_Msg_ID_ADD();
            i = USB302_RX_Buff[2] & 0x70; // bit14~12表示控制消息还是数据消息，控制消息为0，非零为数据消息，表示包含的电压种类
            if (i == 0)                   // 控制消息
            {
                // printf("control message\r\n");
                i = (USB302_RX_Buff[1] & 0x07); // 获取包类型
                switch (i)
                {
                case 1:              // GoodCRC
                    printf("CRC\n"); // GoodCRCGoodCRC
                    break;
                case 3:              // Accept
                    printf("ASK\n"); // Accept
                    break;
                case 4:              // Reject
                    printf("NAK\n"); // Reject
                    break;
                case 6:              // PS_RDY
                    printf("RDY\n"); // PS_RDY
                    break;
                case 8: // Get_Sink_Cap  必须回复点东西
                    DelayMs(1);
                    break;
                default:
                    break;
                }
            }
            else // 数据消息
            {
                // printf("data message\r\n");
                if ((USB302_RX_Buff[1] & 0x07) == 0x01) // Source_Capabilities
                {
                    if (PD_STEP == 0)
                    {
                        i = USB302_RX_Buff[1] & 0xC0; // bit7~6表示PD的版本
                        i >>= 1;
                        if (CCx_PIN_Useful == 1) // 调整PD版本
                        {
                            i |= 0x05;
                        }
                        else if (CCx_PIN_Useful == 2)
                        {
                            i |= 0x06;
                        }

                        USB302_Wite_Reg(0x03, i);
                        USB302_Wite_Reg(0x0C, 0x02); // Reset PD
                        USB302_Wite_Reg(0x07, 0x04);
                        PD_STEP = 1;
                        PD_MSG_ID = 0; // 现在开始正式从0开始记录
                        return;
                    }
                    i = USB302_RX_Buff[2] & 0x70; // bit14~12表示控制消息还是数据消息，控制消息为0，非零为数据消息，表示包含的电压种类
                    i >>= 4;
                    PD_Source_Capabilities_Inf_num = i;

                    for (i = 0; i < PD_Source_Capabilities_Inf_num; i++)
                    {
                        PD_Source_Capabilities_Inf[i].PDC_INF[0] = USB302_RX_Buff[4 * i + 3];
                        PD_Source_Capabilities_Inf[i].PDC_INF[1] = USB302_RX_Buff[4 * i + 4];
                        PD_Source_Capabilities_Inf[i].PDC_INF[2] = USB302_RX_Buff[4 * i + 5];
                        PD_Source_Capabilities_Inf[i].PDC_INF[3] = USB302_RX_Buff[4 * i + 6];
                        if ((PD_Source_Capabilities_Inf[i].PDC_INF[3] & 0xc0) == 0) // bit30-31表示电源类型，0表示FIXED电源，1表示电池，2表示可变电源，3表示PPS
                            j++;
                    }
                    PD_Source_Capabilities_Inf_num = j;
                    printf("Adapter supports %d outputs\n", PD_Source_Capabilities_Inf_num);
                    PD_STEP = 2;
                }
            }
        }
    }
}

// 发送请求 要有objects 号
void USB302_Send_Requse(uint8_t objects)
{
    uint8_t i;
    DelayMs(10);
    if (objects > PD_Source_Capabilities_Inf_num)
        return;
    for (i = 0; i < 14; i++) // 装填发送buff
    {
        USB302_TX_Buff[i] = PD_Resq[i];
    }
    // USB302_TX_Buff[0]~USB302_TX_Buff[5]为FUSB302内部寄存器发送
    USB302_TX_Buff[6] |= PD_MSG_ID << 1;
    USB302_TX_Buff[5] |= PD_Version;

    USB302_TX_Buff[10] |= objects << 4;
    // pps

    USB302_Wite_Reg(0x06, 0x40); // 清发送
    USB302_Wite_FIFO(USB302_TX_Buff, 14);
    USB302_Wite_Reg(0x06, 0x05); // 开始发
    PD_Msg_ID_ADD();             // 加包
}

void USB302_Send_Min_Request(void)
{
    uint8_t pd_buf[6];

    /* -------- PD Header -------- */
    pd_buf[0] = 0x12; // Header LSB: Request, 1 Data Object
    pd_buf[1] = 0x10; // Header MSB: Sink, UFP, PD2.0

    /* -------- Request Data Object -------- */
    pd_buf[2] = 0x64; // Operating Current LSB (1A)
    pd_buf[3] = 0x64; // Max Current LSB (1A)
    pd_buf[4] = 0x10; // PDO index = 1
    pd_buf[5] = 0x00;

    /* -------- TX 必须步骤 -------- */
    USB302_Wite_Reg(0x07, 0x04); // Flush TX FIFO
    USB302_Wite_FIFO(pd_buf, 6); // Write Header + Data
    USB302_Wite_Reg(0x09, 0x01); // Send TX
}

void USB302_Check_TX_Result(void)
{
    uint8_t intA = USB302_Read_Reg(0x3E);
    uint8_t intB = USB302_Read_Reg(0x3F);

    printf("INT_A=%02X INT_B=%02X\r\n", intA, intB);

    if (intA & 0x02) // TX_SUCCESS（以 datasheet 为准）
        printf("PD Request TX SUCCESS\r\n");

    if (intA & 0x01) // TX_FAIL
        printf("PD Request TX FAIL\r\n");
}

void USB302_Get_Data(void)
{
    uint8_t i = 0;
    uint16_t cachevol = 0, cachecur = 0;
    if (PD_STEP == 2)
    {
        USB302_Send_Requse(PD_Source_Capabilities_Inf_num); // 进行一次1包请求
        for (i = 0; i < PD_Source_Capabilities_Inf_num; i++)
        {
            if ((PD_Source_Capabilities_Inf[i].PDC_INF[3] & 0xc0) == 0) // 普通PD手册P154页
            {
                cachecur = ((PD_Source_Capabilities_Inf[i].PDC_INF[1] & 0x03) << 8) | (PD_Source_Capabilities_Inf[i].PDC_INF[0]);        /*****读取一组电流******/
                cachecur *= 10;                                                                                                          // 10ma
                cachevol = ((PD_Source_Capabilities_Inf[i].PDC_INF[2] & 0x0F) << 8) | (PD_Source_Capabilities_Inf[i].PDC_INF[1] & 0xFC); /*****Read voltage******/
                cachevol >>= 2;
                cachevol *= 50; // 50mv
                printf("Voltage: %dV\n", cachevol / 1000);
                printf("Current: %d mA\n", cachecur);
            }
        }
        PD_STEP = 3;
    }
}
