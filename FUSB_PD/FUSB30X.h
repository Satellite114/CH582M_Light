#ifndef __FUSB30X_INC__
#define __FUSB30X_INC__
#include "CH58x_common.h"
#include <stdio.h>
#define FUSB30X_READ_ADDR 0x44  // 读器件指令
#define FUSB30X_WRITE_ADDR 0x44 // 写器件指令
#define FUSB302_I2C_ADDR 0x22 << 1
typedef struct
{
  uint8_t PDC_INF[4];
} PD_Source_Capabilities_TypeDef;
void i2c_transfer_data(uint8_t addr, uint8_t data_len, uint8_t *data);
void i2c_recv_data(uint8_t addr, uint8_t data_len, uint8_t *data);
// #define FUSB30Xint_GPIO_Port FUSB_INT_GPIO_Port
// #define FUSB30Xint_GPIO FUSB_INT_Pin

// #define READ_FUSB30X_INT HAL_GPIO_ReadPin(FUSB30Xint_GPIO_Port, FUSB30Xint_GPIO)


// INT 引脚改为 PB5
#define FUSB30Xint_GPIO_Port  GPIOB
#define FUSB30Xint_GPIO       GPIO_Pin_5

// 读取 INT 引脚状态
#define READ_FUSB30X_INT      (GPIOB_ReadPortPin(GPIO_Pin_5) ? 1 : 0)

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

/* 函数声明 */
uint8_t USB302_Init(void);
void Check_USB302(void);
void USB302_Get_Data(void);       // 获取PD档位信息（需在PD_STEP==2时调用）
void USB302_Data_Service(void);   // 数据服务
void USB302_Send_Requse(uint8_t req_num);  // 发送PD请求

/* 导出全局变量（供UI使用） */
extern PD_Source_Capabilities_TypeDef PD_Source_Capabilities_Inf[7];
extern uint8_t PD_Source_Capabilities_Inf_num;
extern uint8_t PD_STEP;  // PD初始化步骤（2=可获取档位，3=已获取）

/**
 * @brief   解析PD档位信息
 * @param   index: 档位索引(0~6)
 * @param   voltage_mv: 输出电压(mV)
 * @param   current_ma: 输出电流(mA)
 * @return  0=成功, 1=无效索引
 */
uint8_t USB302_Parse_PDO(uint8_t index, uint16_t *voltage_mv, uint16_t *current_ma);

#endif
