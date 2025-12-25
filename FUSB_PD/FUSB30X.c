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


void i2c_transfer_data(uint8_t addr, uint8_t data_len, uint8_t *data)
{
    uint8_t i = 0;

    // 等待总线空闲
    while (I2C_GetFlagStatus(I2C_FLAG_BUSY) != RESET);

    // 生成 START
    I2C_GenerateSTART(ENABLE);
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_MODE_SELECT));

    // 发送从机地址 + 写
    I2C_Send7bitAddress(addr, I2C_Direction_Transmitter);
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));

    // 发送数据
    for (i = 0; i < data_len; i++)
    {
        while (I2C_GetFlagStatus(I2C_FLAG_TXE) == RESET); // 等待 TXE
        I2C_SendData(data[i]);
    }

    // 等待最后一个字节发送完成
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    // 生成 STOP
    I2C_GenerateSTOP(ENABLE);
}

void i2c_recv_data(uint8_t addr, uint8_t data_len, uint8_t *data)
{
    uint8_t i = 0;

    // 等待总线空闲
    while (I2C_GetFlagStatus(I2C_FLAG_BUSY) != RESET);

    // 生成 START
    I2C_GenerateSTART(ENABLE);
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_MODE_SELECT));

    // 发送从机地址 + 读
    I2C_Send7bitAddress(addr, I2C_Direction_Receiver);
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED));

    // 单字节读取
    if (data_len == 1)
    {
        I2C_AcknowledgeConfig(DISABLE); // 1字节无需ACK
        I2C_GenerateSTOP(ENABLE);       // STOP在读取前生成
        while (I2C_GetFlagStatus(I2C_FLAG_RXNE) == RESET);
        data[0] = I2C_ReceiveData();
        I2C_AcknowledgeConfig(ENABLE);
    }
    else
    {
        // 多字节读取
        for (i = 0; i < data_len; i++)
        {
            if (i == data_len - 1) // 倒数第1字节
            {
                I2C_AcknowledgeConfig(DISABLE); 
                I2C_GenerateSTOP(ENABLE);
            }

            while (I2C_GetFlagStatus(I2C_FLAG_RXNE) == RESET);
            data[i] = I2C_ReceiveData();
        }
        I2C_AcknowledgeConfig(ENABLE);
    }
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

uint8_t USB302_Read_Reg(uint8_t REG_ADDR)
{

   uint8_t val = 0;

    /* 等待 I2C 空闲 */
    while (I2C_GetFlagStatus(I2C_FLAG_BUSY) != RESET);

    /* START */
    I2C_GenerateSTART(ENABLE);
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_MODE_SELECT));

    /* 发送器件地址 + 写 */
    I2C_Send7bitAddress(FUSB302_I2C_ADDR, I2C_Direction_Transmitter);
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));

    /* 发送寄存器地址 */
    I2C_SendData(REG_ADDR);
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    /* Repeated START */
    I2C_GenerateSTART(ENABLE);
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_MODE_SELECT));

    /* 发送器件地址 + 读 */
    I2C_Send7bitAddress(FUSB302_I2C_ADDR, I2C_Direction_Receiver);
    while (!I2C_CheckEvent(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED));

    /* 只读 1 字节 */
    I2C_GenerateSTOP(ENABLE);
    while (I2C_GetFlagStatus(I2C_FLAG_RXNE) == RESET);
    val = I2C_ReceiveData();

    return val;
}

void USB302_Wite_Reg(uint8_t addr, uint8_t val)
{
    uint8_t buf[2];
    buf[0] = addr;  // 第一个字节是寄存器地址
    buf[1] = val;   // 第二个字节是要写入的值
    
    i2c_transfer_data(FUSB302_I2C_ADDR, 2, buf);
}

void USB302_Read_FIFO(uint8_t *pBuf, uint8_t len)
{
    const uint8_t FIFO_ADDR = 0x43;

    i2c_transfer_data(FUSB302_I2C_ADDR, 1, &FIFO_ADDR);
    i2c_recv_data(FUSB302_I2C_ADDR, len, pBuf);
}

void USB302_Wite_FIFO(uint8_t *data, uint8_t length)
{
    const uint8_t FIFO_ADDR = 0x43;
    uint8_t buf[length + 1];

    // 第 0 个字节放寄存器地址
    buf[0] = FIFO_ADDR;

    // 后续字节放要写入的数据
    for (uint8_t i = 0; i < length; i++)
    {
        buf[i + 1] = data[i];
    }

    // 调用 CH582M I2C 写函数
    // i2c_transfer_data(addr, 数据长度, 数据指针)
    i2c_transfer_data(FUSB302_I2C_ADDR, length + 1, buf);
}

void Check_USB302(void)
{
    uint8_t Read_Back;
    Read_Back = USB302_Read_Reg(0x01);
    printf("Read_Back FUSB302 chip ID :0X%x\n", Read_Back);
    if (Read_Back ==0x91) // 读到的ID只可能是0x80 0x81 0x82 中的一个 代表版本号
        printf("Check_USB302 IS OK \n");
    else
        printf("Check_USB302 IS ERR \n");
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
  Read_State=USB302_Read_Reg(0x40);//读状态
  USB302_Wite_Reg(0x02, 0x03);//切换到初始状态
  Read_State&=0x03;//只看低2位 看主机有没有电压
  if(Read_State>0)
  { 
    CCx_PIN_Useful=1;
    return 1;
  }
  USB302_Wite_Reg(0x02, 0x0B); // Switch on MEAS_CC2
  DelayMs(2);  
  Read_State=USB302_Read_Reg(0x40);//读状态
  USB302_Wite_Reg(0x02, 0x03);//切换到初始状态
  Read_State&=0x03;//只看低2位 看主机有没有电压

  if(Read_State>0)
  { 
    CCx_PIN_Useful=2;
    return 1;
  } 
  return 0;
}

//返回 0 失败， 1 成功
uint8_t USB302_Init(void)
{  
  printf("Checking PD UFP..\n");
  if(USB302_Chech_CCx()==0)return 0;//检查有没有接着设备
  USB302_Wite_Reg(0x09, 0x40);//发送硬件复位包
  USB302_Wite_Reg(0x0C, 0x03); // Reset FUSB302
  DelayMs(5);  
  USB302_Wite_Reg(0x09, 0x07);//使能自动重试 3次自动重试
  USB302_Wite_Reg(0x0E, 0xFC);//使能各种中断
  //USB302_Wite_Reg(0x0F, 0xFF);
  USB302_Wite_Reg(0x0F, 0x01);
  USB302_Wite_Reg(0x0A, 0xEF);
  USB302_Wite_Reg(0x06, 0x00);//清空各种状态
  USB302_Wite_Reg(0x0C, 0x02);//复位PD
  if(CCx_PIN_Useful==1)
  {
    //USB302_Wite_Reg(0x02, 0x07); // Switch on MEAS_CC1
    USB302_Wite_Reg(0x02, 0x05); // Switch on MEAS_CC1
    USB302_Wite_Reg(0x03, 0x41); // Enable BMC Tx on_CC1 PD3.0
    //USB302_Wite_Reg(0x03, 0x45); // Enable BMC Tx on_CC1 PD3.0 AutoCRC
  }
  else if(CCx_PIN_Useful==2)
  {
    //USB302_Wite_Reg(0x02, 0x0B); // Switch on MEAS_CC2 
    USB302_Wite_Reg(0x02, 0x0A); // Switch on MEAS_CC2 
    USB302_Wite_Reg(0x03, 0x42); // Enable BMC Tx on CC2 PD3.0
    //USB302_Wite_Reg(0x03, 0x46); // Enable BMC Tx on_CC1 PD3.0 AutoCRC
  }
  USB302_Wite_Reg(0x0B, 0x0F);//全电源
  USB302_Read_Reg(0x3E);
  USB302_Read_Reg(0x3F);
  USB302_Read_Reg(0x42);
  RX_Length=0;
  PD_STEP=0;
  PD_Source_Capabilities_Inf_num=0;  
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
                 printf("control message\r\n");
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
                 printf("data message\r\n");
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
                    printf("该适配器支持%d种输出\n", PD_Source_Capabilities_Inf_num);
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
                cachevol = ((PD_Source_Capabilities_Inf[i].PDC_INF[2] & 0x0F) << 8) | (PD_Source_Capabilities_Inf[i].PDC_INF[1] & 0xFC); /*****读取一组电压******/
                cachevol >>= 2;
                cachevol *= 50; // 50mv
                printf("cachevol %dV\n", cachevol / 1000);
                printf("cachecur %fA\n", (float)cachecur / 1000.0);
            }
        }
        PD_STEP = 3;
    }
}
