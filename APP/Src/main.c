/********************************** (C) COPYRIGHT *******************************
 * File Name          : Main.c
 * Author             : WCH
 * Version            : V1.0
 * Date               : 2020/08/06
 * Description        : I2C???????
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

#include "CCHandshake.h"
#include "CH58x_common.h"
#include "FUSB30X.h"
/**
 * @brief  ???I2C???????????????????????????
 */
void HW_I2C_Init()
{
    GPIOPinRemap(DISABLE, RB_PIN_I2C);
    GPIOB_ModeCfg(GPIO_Pin_12 | GPIO_Pin_13, GPIO_ModeIN_PU);
    I2C_Init(I2C_Mode_I2C, 10000, I2C_DutyCycle_16_9, I2C_Ack_Enable, I2C_AckAddr_7bit, 0x22);
    I2C_StretchClockCmd(ENABLE);
    I2C_Cmd(ENABLE);
}

/**
 * @brief  FUSB302????IIC GPIO?????
 *         SCL: PB13
 *         SDA: PB12
 *         INT: PB5
 */
void FUSB302_IIC_GPIO_Init(void)
{
    /* ???? SCL (PB13) ?? SDA (PB12) ?????????5mA???? */
    GPIOB_ModeCfg(GPIO_Pin_12 | GPIO_Pin_13, GPIO_ModeOut_PP_5mA);
    GPIOB_SetBits(GPIO_Pin_12 | GPIO_Pin_13);  // ????????
    
    /* ???? INT ???? (PB5) ????????? */
    GPIOB_ModeCfg(GPIO_Pin_5, GPIO_ModeIN_PU);
    
    /* ?????????IIC */
    fusb302_iic_init();
    
    printf("FUSB302 ????????IIC???\n");
    printf("  SCL: PB13\n");
    printf("  SDA: PB12\n");
    printf("  INT: PB5\n");
}
int main()
{

     SetSysClock(CLK_SOURCE_PLL_60MHz);
    //SetSysClock(CLK_SOURCE_PLL_30MHz);

    // ?????????
    GPIOA_SetBits(bTXD1);
    GPIOA_ModeCfg(bTXD1, GPIO_ModeOut_PP_5mA);
    UART1_DefInit();

    // ?????FUSB302????IIC
    FUSB302_IIC_GPIO_Init();

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

    while (USB302_Init() == 0) // ???????0 ?????????? ???????
    {
        DelayMs(100);
        printf("WAITTING \n");
    }
    printf("connected \n");

    // uint16_t timeout_counter = 0;
    while (1)
    {
        USB302_Data_Service();
        USB302_Get_Data();

        // if (PD_STEP == 5)
        // {
        //     // PD???????
        //     printf("=== PD????????? ===\n");
        //     break;
        // }
        // else if (PD_STEP == 99)
        // {
        //     // PD???????
        //     printf("=== PD??????? ===\n");
        //     break;
        // }

        // // ??????????30??
        // timeout_counter++;
        // if (timeout_counter > 30000)
        // {
        //     printf("=== PD?????? ===\n");
        //     printf("???PD_STEP=%d\n", PD_STEP);
        //     break;
        // }

        // DelayMs(1); // ?????????1ms?????????
    }

    while (1)
    {
        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */
        if (PD_STEP == 5)
        {
            printf("PD????????????????...\r\n");
        }
        else
        {
            printf("PD????????????: %d\r\n", PD_STEP);
        }

        DelayMs(1000);
    }
}
