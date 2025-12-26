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
// #define FUSB302_TEST
#define BLUE_PWM_TEST
#ifdef FUSB302_TEST
#include "CH58x_common.h"
#include "FUSB30X.h"
/**
 * @brief  FUSB302 GPIO INIT
 *         SCL: PB13
 *         SDA: PB12
 *         INT: PB5
 */
void FUSB302_IIC_GPIO_Init(void)
{

    GPIOB_ModeCfg(GPIO_Pin_12 | GPIO_Pin_13, GPIO_ModeOut_PP_5mA);
    GPIOB_SetBits(GPIO_Pin_12 | GPIO_Pin_13);

    GPIOB_ModeCfg(GPIO_Pin_5, GPIO_ModeIN_PU);

    fusb302_iic_init();

    printf("FUSB302 IIC Init\n");
    printf("  SCL: PB13\n");
    printf("  SDA: PB12\n");
    printf("  INT: PB5\n");
}

int main()
{

    SetSysClock(CLK_SOURCE_PLL_60MHz);

    GPIOA_SetBits(bTXD1);
    GPIOA_ModeCfg(bTXD1, GPIO_ModeOut_PP_5mA);
    UART1_DefInit();

    FUSB302_IIC_GPIO_Init();

    Check_USB302();

    while (USB302_Init() == 0)
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
        {
            printf("handshake OK \n");
            break;
        }
    }

    while (1)
    {
        /* USER CODE END WHILE */
        printf("FSUB302 test success \n");
        DelayMs(1000);
    }
}
#endif
#ifdef BLUE_PWM_TEST
/********************************** (C) COPYRIGHT *******************************
 * File Name          : main_ble_pwm.c
 * Author             : WCH
 * Version            : V1.0
 * Date               : 2025/12/26
 * Description        : 蓝牙PWM控制主程序
 *                      通过蓝牙串口接收2字节数据控制PWM4和PWM5输出
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

#include "CONFIG.h"
#include "HAL.h"
#include "PWM.h"
#include "peripheral.h"
#include "CH58x_common.h"
#include "FUSB30X.h"
/*********************************************************************
 * GLOBAL VARIABLES
 */
__attribute__((aligned(4))) uint32_t MEM_BUF[BLE_MEMHEAP_SIZE / 4];

#if (defined(BLE_MAC)) && (BLE_MAC == TRUE)
const uint8_t MacAddr[6] = {0x84, 0xC2, 0xE4, 0x03, 0x02, 0x02};
#endif

/*********************************************************************
 * @fn      Main_Circulation
 *
 * @brief   主循环
 *
 * @return  none
 */
__HIGH_CODE
__attribute__((noinline)) void Main_Circulation()
{
    while (1)
    {
        TMOS_SystemProcess();
    }
}
void FUSB302_IIC_GPIO_Init(void)
{

    GPIOB_ModeCfg(GPIO_Pin_12 | GPIO_Pin_13, GPIO_ModeOut_PP_5mA);
    GPIOB_SetBits(GPIO_Pin_12 | GPIO_Pin_13);

    GPIOB_ModeCfg(GPIO_Pin_5, GPIO_ModeIN_PU);

    fusb302_iic_init();

    printf("FUSB302 IIC Init\n");
    printf("  SCL: PB13\n");
    printf("  SDA: PB12\n");
    printf("  INT: PB5\n");
}
/*********************************************************************
 * @fn      main
 *
 * @brief   主函数
 *
 * @return  none
 */
int main(void)
{
#if (defined(DCDC_ENABLE)) && (DCDC_ENABLE == TRUE)
    PWR_DCDCCfg(ENABLE);
#endif
    SetSysClock(CLK_SOURCE_PLL_60MHz);
#if (defined(HAL_SLEEP)) && (HAL_SLEEP == TRUE)
    GPIOA_ModeCfg(GPIO_Pin_All, GPIO_ModeIN_PU);
    GPIOB_ModeCfg(GPIO_Pin_All, GPIO_ModeIN_PU);
#endif
#ifdef DEBUG
    GPIOA_SetBits(bTXD1);
    GPIOA_ModeCfg(bTXD1, GPIO_ModeOut_PP_5mA);
    UART1_DefInit();
#endif
    FUSB302_IIC_GPIO_Init();

    Check_USB302();

    while (USB302_Init() == 0)
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
        {
            printf("handshake OK \n");
            break;
        }
    }

    PRINT("%s\n", VER_LIB);

    // 初始化PWM模块
    PRINT("Initializing PWM...\n");
    PWM_ComplementaryInit();
    PRINT("PWM initialized\n");

    // 设置初始PWM状态（可选）
    PWM_SetDutyAndBalance(0, 0); // 初始状态：关闭

    // 初始化蓝牙
    CH58X_BLEInit();
    HAL_Init();
    GAPRole_PeripheralInit();
    Peripheral_Init();

    PRINT("BLE PWM Control System Started\n");
    PRINT("Waiting for BLE connection...\n");

    Main_Circulation();
}

/******************************** endfile @ main ******************************/

#endif
