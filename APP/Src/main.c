/********************************** (C) COPYRIGHT *******************************
 * File Name          : Main.c
 * Author             : WCH
 * Version            : V1.0
 * Date               : 2020/08/06
 * Description        : I2C功能演示
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

#include "CH58x_common.h"
#include "FUSB30X.h"
#include "CCHandshake.h"
void HW_I2C_Init()
{
    GPIOPinRemap(DISABLE, RB_PIN_I2C);
    GPIOB_ModeCfg(GPIO_Pin_12 | GPIO_Pin_13, GPIO_ModeIN_PU);
    I2C_Init(I2C_Mode_I2C, 10000, I2C_DutyCycle_16_9, I2C_Ack_Enable, I2C_AckAddr_7bit, 0x22);
    I2C_StretchClockCmd(ENABLE);
    I2C_Cmd(ENABLE);
}
int main()
{

    SetSysClock(CLK_SOURCE_PLL_60MHz);

    // 初始化串口
    GPIOA_SetBits(bTXD1);
    GPIOA_ModeCfg(bTXD1, GPIO_ModeOut_PP_5mA);
    UART1_DefInit();

    HW_I2C_Init();


    // uint8_t buf = 0x01;

    // volatile uint8_t id = 0;

    // while (1)
    // {
    //     i2c_transfer_data(0x22 << 1, 1, &buf);
    //     i2c_recv_data(0x22 << 1, 1, &id);
    //     printf("FUSB302 ID:0x%x\n", id);
    //     DelayMs(1000);
    // }

    Check_USB302();

    while (USB302_Init() == 0) // 如果初始化一直是0 就说明没插入啥 等
    {
        DelayMs(100);
        printf("WAITTING \n");
    }
    printf("connected \n");

    while (1)
    {
        USB302_Data_Service();
        USB302_Get_Data();
        if (PD_STEP == 3)
            break;
        printf("PD_STEP=%d\n", PD_STEP);

        DelayMs(100);
    }

  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    printf("PD test success \r\n");

    DelayMs(1000);


  }
}
