/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Author             : WCH
 * Version            : V1.1
 * Date               : 2020/08/06
 * Description        : 外设从机应用主函数及任务系统初始化
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

/******************************************************************************/
/* 头文件包含 */
#include "CONFIG.h"
#include "HAL.h"
#include "peripheral.h"
#include "app_uart.h"
#include "PWM.h"

/*********************************************************************
 * GLOBAL TYPEDEFS
 */
__attribute__((aligned(4))) u32 MEM_BUF[BLE_MEMHEAP_SIZE / 4];

#if (defined(BLE_MAC)) && (BLE_MAC == TRUE)
u8C MacAddr[6] = {0x84, 0xC2, 0xE4, 0x03, 0x02, 0x02};
#endif

/*******************************************************************************
 * Function Name  : Main_Circulation
 * Description    : 主循环
 * Input          : None
 * Output         : None
 * Return         : None
 *******************************************************************************/
__HIGH_CODE
__attribute__((noinline)) void Main_Circulation()
{
    while (1) {
        TMOS_SystemProcess();
        app_uart_process();
    }
}

/*******************************************************************************
 * Function Name  : main
 * Description    : 主函数
 * Input          : None
 * Output         : None
 * Return         : None
 *******************************************************************************/
int main(void)
{
#if (defined(DCDC_ENABLE)) && (DCDC_ENABLE == TRUE)
    PWR_DCDCCfg(ENABLE);
#endif
    SetSysClock(CLK_SOURCE_PLL_60MHz);
// 注释掉HAL_SLEEP的GPIO配置，避免覆盖PWM引脚
// #if (defined(HAL_SLEEP)) && (HAL_SLEEP == TRUE)
//     GPIOA_ModeCfg(GPIO_Pin_All, GPIO_ModeIN_PU);
//     GPIOB_ModeCfg(GPIO_Pin_All, GPIO_ModeIN_PU);
// #endif
#ifdef DEBUG
    // 将UART1从PA8/PA9重映射到PB12/PB13，避免与PA9上的TMR0 PWM冲突
    GPIOPinRemap(ENABLE, RB_PIN_UART1);

    // 配置PB13为UART1 TXD1_输出，PB12为UART1 RXD1_输入
    GPIOB_SetBits(bTXD1_);
    GPIOB_ModeCfg(bTXD1_, GPIO_ModeOut_PP_5mA);
    GPIOB_ModeCfg(bRXD1_, GPIO_ModeIN_PU);

    UART1_DefInit();
    DelayMs(10); // 等待串口稳定
#endif

    //PB6_PWMX_80kHz_50Duty_Start();

    PRINT("\r\n=== CH582M PWM Test Start ===\r\n");
    PRINT("System Clock: %d Hz\r\n", GetSysClock());
    PRINT("Calling PWM_ComplementaryInit()...\r\n");

    // 初始化PWM
    PWM_ComplementaryInit();

    PRINT("PWM_ComplementaryInit() returned\r\n");

    // 设置总占空比50%，balance=0（完全平衡）
    PRINT("Setting PWM: Total=50%%, Balance=0\r\n");
    PWM_SetDutyAndBalance(50, 0);  // PWM1=25%, PWM2=25%

    // // 延时一下，让PWM稳定输出
    // DelayMs(500);

    // // 运行完整测试
    //  PWM_Test();

    // PRINT("\r\n=== Test Complete, entering main loop ===\r\n");

    // 主循环，保持PWM运行
    while (1) {
        DelayMs(1000);
    }

    // PRINT("%s\\n", VER_LIB);
    // CH58X_BLEInit();
    // HAL_Init();
    // GAPRole_PeripheralInit();
    // Peripheral_Init();
    // app_uart_init();
    // Main_Circulation();
}

/******************************** endfile @ main ******************************/
