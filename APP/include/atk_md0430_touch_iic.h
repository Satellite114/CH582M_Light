/**
 ****************************************************************************************************
 * @file        atk_md0430_touch_iic.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2022-06-21
 * @brief       ATK-MD0430模块触摸IIC接口驱动代码
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 STM32F103开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 *
 ****************************************************************************************************
 */

#ifndef __ATK_MD0430_TOUCH_IIC_H
#define __ATK_MD0430_TOUCH_IIC_H
// GPIOB_SetBits SCL PB13
#include "CH58x_common.h"

#include "stdint.h"
/* IO操作 */
#define ATK_MD0430_TOUCH_IIC_SCL(x)                 do{ x ?                                                                                                         \
                                                        GPIOB_SetBits(GPIO_Pin_13) :    \
                                                        GPIOB_ResetBits(GPIO_Pin_13);   \
                                                    }while(0)

#define ATK_MD0430_TOUCH_IIC_SDA(x)                 do{ x ?                                                                                                         \
                                                        GPIOB_SetBits(GPIO_Pin_12) :    \
                                                        GPIOB_ResetBits(GPIO_Pin_12);   \
                                                    }while(0)

#define ATK_MD0430_TOUCH_IIC_READ_SDA()            GPIOB_ReadPortPin(GPIO_Pin_12)

/* 错误代码 */
#define ATK_MD0430_TOUCH_IIC_EOK    0   /* 没有错误 */
#define ATK_MD0430_TOUCH_IIC_ERROR  1   /* 错误 */

/* 操作函数 */
void atk_md0430_touch_iic_init(uint8_t iic_addr);                                   /* 初始化IIC接口 */
uint8_t atk_md0430_touch_iic_write_reg(uint16_t reg, uint8_t *buf, uint8_t len);    /* 写ATK-MD0430模块触摸寄存器 */
void atk_md0430_touch_iic_read_reg(uint16_t reg, uint8_t *buf, uint8_t len);        /* 读ATK-MD0430模块触摸寄存器 */

#endif /* ATK_MD0430_USING_TOUCH */