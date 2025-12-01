/********************************** (C) COPYRIGHT *******************************
 * File Name          : PWM.c
 * Author             : 
 * Version            : V1.0
 * Date               : 2025/12/01
 * Description        : 互补PWM控制模块实现
 *                      实现两个独立PWM的占空比控制，总占空比可调，
 *                      两个PWM之间的分配比例可调
 *******************************************************************************/

#include "PWM.h"
#include "CH58x_common.h"
#include <stdio.h>

#define PWM_SOFT_STEPS 10   // 软件PWM步数，用于提高输出频率

/*********************************************************************
 * GLOBAL VARIABLES
 */

// 当前总占空比 (0-100)
static uint8_t g_total_duty = 0;

// 当前平衡度 (-100 到 +100)
static int8_t g_balance = 0;

// PWM1和PWM2的实际占空比 (0-100)
static uint8_t g_duty1 = 0;
static uint8_t g_duty2 = 0;

// PWM周期计数器 (0-99)，100步分辨率
static volatile uint8_t g_pwm_counter = 0;

// PWM通道切换点
static uint8_t g_pwm1_end = 0;   // PWM1结束点
static uint8_t g_pwm2_end = 0;   // PWM2结束点

// TMR0是否已经启动的标志
static uint8_t g_pwm_started = 0;

/*********************************************************************
 * LOCAL FUNCTIONS
 */

/*********************************************************************
 * @fn      PWM_UpdateOutput
 *
 * @brief   根据总占空比和平衡度计算并更新PWM参数
 *          计算公式：
 *          duty1 = total_duty * (1 + balance/100) / 2
 *          duty2 = total_duty * (1 - balance/100) / 2
 *          
 *          互补输出：PWM1在0-duty1时间高电平
 *                   PWM2在duty1到duty1+duty2时间高电平
 *
 * @return  None
 */
static void PWM_UpdateOutput(void)
{
    int16_t temp_duty1, temp_duty2;
    
    // 限制总占空比范围 0-100
    if(g_total_duty > 100)
    {
        g_total_duty = 100;
    }
    
    // 限制平衡度范围 -100到+100
    if(g_balance > 100)
    {
        g_balance = 100;
    }
    else if(g_balance < -100)
    {
        g_balance = -100;
    }
    
    // 计算两个PWM的占空比
    // duty1 = total_duty * (100 + balance) / 200
    // duty2 = total_duty * (100 - balance) / 200
    temp_duty1 = ((int16_t)g_total_duty * (100 + g_balance)) / 200;
    temp_duty2 = ((int16_t)g_total_duty * (100 - g_balance)) / 200;
    
    // 限制范围
    if(temp_duty1 < 0) temp_duty1 = 0;
    if(temp_duty1 > 100) temp_duty1 = 100;
    if(temp_duty2 < 0) temp_duty2 = 0;
    if(temp_duty2 > 100) temp_duty2 = 100;
    
    g_duty1 = (uint8_t)temp_duty1;
    g_duty2 = (uint8_t)temp_duty2;
    
    // 将占空比百分比映射到 0~PWM_SOFT_STEPS 的步数
    g_pwm1_end = (g_duty1 * PWM_SOFT_STEPS) / 100;                 // PWM1从0到g_pwm1_end
    g_pwm2_end = ((g_duty1 + g_duty2) * PWM_SOFT_STEPS) / 100;     // PWM2从g_pwm1_end到g_pwm2_end
    
    if(g_pwm1_end > PWM_SOFT_STEPS) g_pwm1_end = PWM_SOFT_STEPS;
    if(g_pwm2_end > PWM_SOFT_STEPS) g_pwm2_end = PWM_SOFT_STEPS;
}

/*********************************************************************
 * @fn      PWM_TimerStart
 *
 * @brief   启动用于软件PWM的TMR0，只调用一次
 *
 * @return  None
 */
static void PWM_TimerStart(void)
{
    // 使用官方风格初始化 TMR0：高速定时器，用于驱动软件PWM
    // 目标：约 80kHz PWM，总步进数=PWM_SOFT_STEPS=10
    // TMR0 中断频率 f_int = 80kHz * 10 = 800kHz
    // 计数周期 t = FREQ_SYS / f_int = 60MHz / 800kHz ≈ 75
    TMR0_TimerInit(75);                     // 设置TMR0周期为75个时钟
    TMR0_ITCfg(ENABLE, TMR0_3_IT_CYC_END);  // 使能周期结束中断
    PFIC_EnableIRQ(TMR0_IRQn);              // 使能TMR0中断通道

    PRINT("[PWM] TMR0 started via TimerInit\r\n");
}

/*********************************************************************
 * PUBLIC FUNCTIONS
 */

/*********************************************************************
 * @fn      PWM_ComplementaryInit
 *
 * @brief   初始化互补PWM控制
 *          使用PWM4和PWM5作为两个独立可调的PWM输出
 *
 * @return  None
 */
void PWM_ComplementaryInit(void)
{
    PRINT("[PWM] Init start\r\n");

    // 配置 PA12 / PA13 为推挽输出，用作两路PWM输出
    GPIOA_ModeCfg(GPIO_Pin_12 | GPIO_Pin_13, GPIO_ModeOut_PP_5mA);
    GPIOA_ResetBits(GPIO_Pin_12 | GPIO_Pin_13);

    // 初始化内部状态变量
    g_total_duty  = 0;
    g_balance     = 0;
    g_duty1       = 0;
    g_duty2       = 0;
    g_pwm_counter = 0;
    g_pwm1_end    = 0;
    g_pwm2_end    = 0;
    g_pwm_started = 0;

    PRINT("[PWM] Vars initialized\r\n");
}


/*********************************************************************
 * @fn      PWM_SetTotalDuty
 *
 * @brief   设置总占空比
 *          两个PWM的占空比之和等于总占空比
 *
 * @param   duty - 总占空比，范围0-100 (%)
 *
 * @return  None
 */
void PWM_SetTotalDuty(uint8_t duty)
{
    g_total_duty = duty;
    PWM_UpdateOutput();
}

/*********************************************************************
 * @fn      PWM_SetBalance
 *
 * @brief   设置平衡度
 *          控制两个PWM之间的分配关系
 *
 * @param   balance - 平衡度，范围-100到+100
 *                    0: 完全平衡
 *                    +100: PWM1=total_duty, PWM2=0
 *                    -100: PWM1=0, PWM2=total_duty
 *
 * @return  None
 */
void PWM_SetBalance(int8_t balance)
{
    g_balance = balance;
    PWM_UpdateOutput();
}

/*********************************************************************
 * @fn      PWM_SetDutyAndBalance
 *
 * @brief   同时设置总占空比和平衡度
 *
 * @param   total_duty - 总占空比，范围0-100 (%)
 * @param   balance    - 平衡度，范围-100到+100
 *
 * @return  None
 */
void PWM_SetDutyAndBalance(uint8_t total_duty, int8_t balance)
{
    g_total_duty = total_duty;
    g_balance = balance;
    PWM_UpdateOutput();

    // 在第一次设置到非零占空比时启动TMR0和中断
    if(!g_pwm_started && (g_pwm1_end > 0 || g_pwm2_end > 0))
    {
        PWM_TimerStart();
        g_pwm_started = 1;
    }

    PRINT("[PWM] Set: Total=%d%%, Balance=%d, PWM1_end=%d, PWM2_end=%d\r\n", 
          g_total_duty, g_balance, g_pwm1_end, g_pwm2_end);
}

/*********************************************************************
 * @fn      PWM_GetActualDuty
 *
 * @brief   获取当前PWM1和PWM2的实际占空比
 *
 * @param   duty1 - 输出PWM1的占空比 (0-100%)
 * @param   duty2 - 输出PWM2的占空比 (0-100%)
 *
 * @return  None
 */
void PWM_GetActualDuty(uint8_t *duty1, uint8_t *duty2)
{
    if(duty1 != NULL)
    {
        *duty1 = g_duty1;
    }
    if(duty2 != NULL)
    {
        *duty2 = g_duty2;
    }
}

/*********************************************************************
 * @fn      PWM_Test
 *
 * @brief   PWM测试函数
 *          测试不同总占空比和平衡度组合下的PWM输出
 *          通过串口输出测试结果
 *
 * @return  None
 */
void PWM_Test(void)
{
    uint8_t duty1, duty2;
    int8_t balance;
    uint8_t total_duty;
    
    PRINT("\r\n========== PWM Complementary Control Test ==========\r\n");
    
    // 测试1: 固定总占空比50%，调整balance从-100到+100
    PRINT("\r\n[Test 1] Total Duty = 50%%, Balance varies\r\n");
    PRINT("Balance\tPWM1%%\tPWM2%%\tSum%%\r\n");
    PRINT("-------\t-----\t-----\t----\r\n");
    
    total_duty = 50;
    for(balance = -100; balance <= 100; balance += 25)
    {
        PWM_SetDutyAndBalance(total_duty, balance);
        PWM_GetActualDuty(&duty1, &duty2);
        PRINT("%d\t%d\t%d\t%d\r\n", balance, duty1, duty2, duty1 + duty2);
        
        // 延时，方便观察波形
        DelayMs(1000);
    }
    
    // 测试2: 固定balance=0，调整总占空比从0到100%
    PRINT("\r\n[Test 2] Balance = 0 (balanced), Total Duty varies\r\n");
    PRINT("Total%%\tPWM1%%\tPWM2%%\tSum%%\r\n");
    PRINT("------\t-----\t-----\t----\r\n");
    
    balance = 0;
    for(total_duty = 0; total_duty <= 100; total_duty += 20)
    {
        PWM_SetDutyAndBalance(total_duty, balance);
        PWM_GetActualDuty(&duty1, &duty2);
        PRINT("%d\t%d\t%d\t%d\r\n", total_duty, duty1, duty2, duty1 + duty2);
        
        // 延时，方便观察波形
        DelayMs(1000);
    }
    
    // 测试3: 不同总占空比和balance的组合
    PRINT("\r\n[Test 3] Various combinations\r\n");
    PRINT("Total%%\tBalance\tPWM1%%\tPWM2%%\tSum%%\r\n");
    PRINT("------\t-------\t-----\t-----\t----\r\n");
    
    // 组合1: 30% duty, +50 balance
    PWM_SetDutyAndBalance(30, 50);
    PWM_GetActualDuty(&duty1, &duty2);
    PRINT("%d\t%d\t%d\t%d\t%d\r\n", 30, 50, duty1, duty2, duty1 + duty2);
    DelayMs(100);
    
    // 组合2: 80% duty, -40 balance
    PWM_SetDutyAndBalance(80, -40);
    PWM_GetActualDuty(&duty1, &duty2);
    PRINT("%d\t%d\t%d\t%d\t%d\r\n", 80, -40, duty1, duty2, duty1 + duty2);
    DelayMs(100);
    
    // 组合3: 100% duty, +100 balance (全部给PWM1)
    PWM_SetDutyAndBalance(100, 100);
    PWM_GetActualDuty(&duty1, &duty2);
    PRINT("%d\t%d\t%d\t%d\t%d\r\n", 100, 100, duty1, duty2, duty1 + duty2);
    DelayMs(100);
    
    // 组合4: 100% duty, -100 balance (全部给PWM2)
    PWM_SetDutyAndBalance(100, -100);
    PWM_GetActualDuty(&duty1, &duty2);
    PRINT("%d\t%d\t%d\t%d\t%d\r\n", 100, -100, duty1, duty2, duty1 + duty2);
    DelayMs(100);
    
    // 测试完成，恢复到默认状态
    PWM_SetDutyAndBalance(50, 0);
    PWM_GetActualDuty(&duty1, &duty2);
    
    PRINT("\r\n[Test Complete] Reset to: Total=50%%, Balance=0\r\n");
    PRINT("Final state: PWM1=%d%%, PWM2=%d%%\r\n", duty1, duty2);
    PRINT("====================================================\r\n\r\n");
}

/*********************************************************************
 * @fn      TMR0_IRQHandler
 *
 * @brief   TMR0中断服务函数
 *          用于生成互补PWM波形
 *          工作原理：
 *          - 计数器从0-99循环
 *          - PWM1在0到g_pwm1_end期间输出高电平
 *          - PWM2在g_pwm1_end到g_pwm2_end期间输出高电平
 *          - 其余时间都输出低电平
 *
 * @return  None
 */
__INTERRUPT
__HIGH_CODE
void TMR0_IRQHandler(void)
{
    // 清除中断标志
    TMR0_ClearITFlag(TMR0_3_IT_CYC_END);
    
    // 控制PWM1输出（PA12）- 在0到g_pwm1_end期间为高
    if(g_pwm_counter < g_pwm1_end)
    {
        GPIOA_SetBits(GPIO_Pin_12);  // PWM1高电平
    }
    else
    {
        GPIOA_ResetBits(GPIO_Pin_12);  // PWM1低电平
    }
    
    // 控制PWM2输出（PA13）- 在g_pwm1_end到g_pwm2_end期间为高
    if(g_pwm_counter >= g_pwm1_end && g_pwm_counter < g_pwm2_end)
    {
        GPIOA_SetBits(GPIO_Pin_13);  // PWM2高电平
    }
    else
    {
        GPIOA_ResetBits(GPIO_Pin_13);  // PWM2低电平
    }
    
    // 更新PWM计数器（循环）
    g_pwm_counter++;
    if(g_pwm_counter >= PWM_SOFT_STEPS)
    {
        g_pwm_counter = 0;
    }
}
