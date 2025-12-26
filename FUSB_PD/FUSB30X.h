#ifndef __FUSB30X_INC__
#define __FUSB30X_INC__
#include "CH58x_common.h"
#include <stdio.h>

/* FUSB302 IIC地址定义 */
#define FUSB302_I2C_ADDR        0x22    // 7位地址

/* IIC引脚定义 - 使用PB13(SCL)和PB12(SDA) */
#define FUSB302_IIC_SCL(x)      do{ x ?                                     \
                                    GPIOB_SetBits(GPIO_Pin_13) :            \
                                    GPIOB_ResetBits(GPIO_Pin_13);           \
                                }while(0)

#define FUSB302_IIC_SDA(x)      do{ x ?                                     \
                                    GPIOB_SetBits(GPIO_Pin_12) :            \
                                    GPIOB_ResetBits(GPIO_Pin_12);           \
                                }while(0)

#define FUSB302_IIC_READ_SDA()  GPIOB_ReadPortPin(GPIO_Pin_12)

/* IIC操作类型 */
#define FUSB302_IIC_WRITE       0
#define FUSB302_IIC_READ        1

typedef struct
{
  uint8_t PDC_INF[4];
} PD_Source_Capabilities_TypeDef;
/* INT 引脚定义 - PB5 */
#define FUSB30Xint_GPIO_Port    GPIOB
#define FUSB30Xint_GPIO         GPIO_Pin_5
#define READ_FUSB30X_INT        (GPIOB_ReadPortPin(GPIO_Pin_5) ? 1 : 0)

typedef enum
{
  TokenTx_TXON = 0xA1,
  TokenTx_SOP1 = 0x12,
  TokenTx_SOP2 = 0x13,
  TokenTx_SOP3 = 0x14, // ??
  TokenTx_RESET1 = 0x15,
  TokenTx_RESET2 = 0x16,
  TokenTx_PACKSYM = 0x80,
  TokenTx_JAM_CRC = 0xFF,
  TokenTx_EOP = 0x14,
  TokenTx_TXOFF = 0xFE,
} TokenTxDef;

/* IIC底层驱动函数 */
void fusb302_iic_init(void);                                                    /* 初始化IIC接口 */
uint8_t fusb302_iic_write_reg(uint8_t reg, uint8_t val);                        /* 写FUSB302寄存器 */
uint8_t fusb302_iic_read_reg(uint8_t reg);                                      /* 读FUSB302寄存器 */
void fusb302_iic_read_fifo(uint8_t *pBuf, uint8_t len);                         /* 读FUSB302 FIFO */
void fusb302_iic_write_fifo(uint8_t *data, uint8_t length);                     /* 写FUSB302 FIFO */

/* FUSB302功能函数 */
uint8_t USB302_Init(void);
void Check_USB302(void);
void USB302_Get_Data(void);                                                     /* 获取PD档位信息（需在PD_STEP==2时调用） */
void USB302_Data_Service(void);                                                 /* 数据服务 */
void USB302_Send_Requse(uint8_t req_num);                                       /* 发送PD请求 */

/* 导出全局变量（供UI使用） */
extern PD_Source_Capabilities_TypeDef PD_Source_Capabilities_Inf[7];
extern uint8_t PD_Source_Capabilities_Inf_num;
extern uint8_t PD_STEP;                                                         /* PD初始化步骤（2=可获取档位，3=已获取） */

/**
 * @brief   解析PD档位信息
 * @param   index: 档位索引(0~6)
 * @param   voltage_mv: 输出电压(mV)
 * @param   current_ma: 输出电流(mA)
 * @return  0=成功, 1=无效索引
 */
uint8_t USB302_Parse_PDO(uint8_t index, uint16_t *voltage_mv, uint16_t *current_ma);

#endif
