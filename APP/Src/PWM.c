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

#define PWM_TMR1_ADV_TICKS 0

/*********************************************************************
 * GLOBAL VARIABLES
 */

// 当前总占空比 (0-100)
static uint8_t g_total_duty = 0;

// 当前平衡度 (-100 到 +100)
static int8_t g_balance = 0;

// PWM1和PWM2的实际占空比 (0-100)
static uint8_t g_duty1 = 0; // A 通道占空比 (%), 对应 PA9 / TMR0 PWM0
static uint8_t g_duty2 = 0; // B 通道占空比 (%), 对应 PB6 / PWMX PWM8

// 记录最近一次计算得到的定时器周期和高电平宽度（tick）
static uint32_t g_period_ticks = 0; // 一个PWM周期对应的定时器计数（与PWMX保持一致）
static uint32_t g_ta_ticks     = 0; // A 高电平宽度（tick）
static uint32_t g_tb_ticks     = 0; // B 高电平宽度（tick）

// 待在TMR1中断中开启的PWM8有效数据宽度（0-255），0表示当前无需开启B通道
static volatile uint8_t g_pwm8_width_pending = 0;

/*********************************************************************
 * LOCAL FUNCTIONS
 */

/*********************************************************************
 * @fn      PWM_UpdateOutput
 *
 * @brief   根据总占空比和平衡度计算并更新PWM参数（占空比百分比）
 *          计算公式：
 *          duty1 = total_duty * (1 + balance/100) / 2
 *          duty2 = total_duty * (1 - balance/100) / 2
 *
 * @return  None
 */
static void PWM_UpdateOutput(void)
{
    int16_t temp_duty1, temp_duty2;

    // 限制总占空比范围 0-100
    if (g_total_duty > 100) {
        g_total_duty = 100;
    }

    // 限制平衡度范围 -100到+100
    if (g_balance > 100) {
        g_balance = 100;
    } else if (g_balance < -100) {
        g_balance = -100;
    }

    // 计算两个PWM的占空比（百分比）
    // duty1 = total_duty * (100 + balance) / 200
    // duty2 = total_duty * (100 - balance) / 200
    temp_duty1 = ((int16_t)g_total_duty * (100 + g_balance)) / 200;
    temp_duty2 = ((int16_t)g_total_duty * (100 - g_balance)) / 200;

    // 限制范围
    if (temp_duty1 < 0) temp_duty1 = 0;
    if (temp_duty1 > 100) temp_duty1 = 100;
    if (temp_duty2 < 0) temp_duty2 = 0;
    if (temp_duty2 > 100) temp_duty2 = 100;

    g_duty1 = (uint8_t)temp_duty1;
    g_duty2 = (uint8_t)temp_duty2;

    // 计算对应的定时器tick数
    // 为了与PWMX(PWM8, PB6)保持同频，这里选用与PWMX一致的周期：
    // PWMX: F_pwm = FREQ_SYS / (3 * 256) ≈ 78.1kHz (当FREQ_SYS=60MHz)
    // 因此TMR0的计数周期也设置为 3*256 个系统时钟
    g_period_ticks = 3 * 256;
    if (g_period_ticks == 0) {
        g_period_ticks = 1;
    }

    g_ta_ticks = (g_period_ticks * g_duty1) / 100; // A高电平宽度
    g_tb_ticks = (g_period_ticks * g_duty2) / 100; // B高电平宽度

    // 防止超过一个周期
    if (g_ta_ticks > g_period_ticks) g_ta_ticks = g_period_ticks;
    if (g_tb_ticks > g_period_ticks) g_tb_ticks = g_period_ticks;
}

// 根据当前g_duty1/g_duty2，直接通过PWMX在PA12(PWM4)、PA13(PWM5)输出PWM
static void PWM_UpdateHardware_PWMX(void)
{
    uint8_t width1 = 0;
    uint8_t width2 = 0;

    if (g_duty1 > 0)
    {
        uint32_t tmp = (PWM_CYCLE_MAX * g_duty1) / 100U;
        if (tmp > 255U) tmp = 255U;
        width1 = (uint8_t)tmp;
    }

    if (g_duty2 > 0)
    {
        uint32_t tmp = (PWM_CYCLE_MAX * g_duty2) / 100U;
        if (tmp > 255U) tmp = 255U;
        width2 = (uint8_t)tmp;
    }

    // 确保PWMX时钟和周期配置为约80kHz
    PWMX_CLKCfg(3);
    PWMX_CycleCfg(PWMX_Cycle_256);

    // 通道1: PWM4 -> PA12
    PWMX_ACTOUT(PWM_CHANNEL_1, width1, High_Level, (width1 ? ENABLE : DISABLE));

    // 通道2: PWM5 -> PA13
    PWMX_ACTOUT(PWM_CHANNEL_2, width2, High_Level, (width2 ? ENABLE : DISABLE));
}

/*********************************************************************
 * @fn      PWM_StopAll
 *
 * @brief   停止TMR0/PWMX的PWM输出
 */
static void PWM_StopAll(void)
{
    // 关闭PWMX的PWM4(PA12)、PWM5(PA13)输出
    PWMX_ACTOUT(PWM_CHANNEL_1, 0, High_Level, DISABLE);
    PWMX_ACTOUT(PWM_CHANNEL_2, 0, High_Level, DISABLE);

    // 关闭TMR1一次性延时定时器，并清除待触发的PWM8宽度
    TMR1_ITCfg(DISABLE, RB_TMR_IE_CYC_END);
    TMR1_Disable();
    TMR1_ClearITFlag(RB_TMR_IF_CYC_END);
    g_pwm8_width_pending = 0;
}

/*********************************************************************
 * @fn      PWM_StartHardware
 *
 * @brief   按当前计算出的 g_period_ticks / g_ta_ticks / g_tb_ticks
 *          配置并启动 TMR0(PA9) 和 PWMX-PWM8(PB6) 的PWM输出。
 *          先启动A( TMR0 )，再延时Ta后启动B( PWM8 )，实现同一周期内先A后B。
 */
static void PWM_StartHardware(void)
{
    uint32_t ta     = g_ta_ticks;
    uint32_t tb     = g_tb_ticks;
    uint32_t period = g_period_ticks;

    // 无高电平时，直接关闭两路PWM
    if ((ta == 0) && (tb == 0)) {
        PWM_StopAll();
        return;
    }

    // 配置PA9(TMR0)为输出引脚，PB6为PWM8输出引脚
    GPIOA_ModeCfg(bTMR0, GPIO_ModeOut_PP_5mA);
    GPIOB_ModeCfg(GPIO_Pin_6, GPIO_ModeOut_PP_5mA);

    // 先停止所有输出
    PWM_StopAll();

    PRINT("[PWM] StartHW: period=%lu, ta=%lu, tb=%lu\r\n", period, ta, tb);

    // 初始化TMR0 PWM：A通道，输出在PA9
    if (ta > 0) {
        TMR0_PWMInit(High_Level, PWM_Times_1);
        TMR0_PWMCycleCfg(period); // 周期
        TMR0_PWMActDataWidth(ta); // A高电平宽度
    }

    // 计算PWM8对应的有效数据宽度（0-256），基于占空比百分比
    uint8_t pwm8_width = 0;
    if (tb > 0) {
        uint32_t tmp = (256U * g_duty2) / 100U;
        if (tmp > 255U) tmp = 255U;
        pwm8_width = (uint8_t)tmp;
    }

    // 若仅有B通道有高电平，直接启动B通道即可
    if ((ta == 0) && (tb > 0)) {
        // 配置PWMX时钟与周期（与PB6_PWMX_80kHz_50Duty_Start一致）
        PWMX_CLKCfg(3);
        PWMX_CycleCfg(PWMX_Cycle_256);

        PWMX_ACTOUT(CH_PWM8, pwm8_width, High_Level, ENABLE);
        return;
    }

    // 如果B没有高电平，则仅启动A通道即可
    if (tb == 0) {
        if (ta > 0) {
            TMR0_PWMEnable();
            TMR0_Enable();
        }
        return;
    }

    // 此时 A、B 都有高电平需求：
    // 使用TMR1一次性定时，在Ta时间后通过中断开启B通道：
    // 1) 先配置并使能TMR1周期结束中断；
    // 2) 调用TMR1_TimerInit(ta)启动计数；
    // 3) 紧接着启动A通道(TMR0)。

    g_pwm8_width_pending = pwm8_width;

    TMR1_Disable();
    TMR1_ClearITFlag(RB_TMR_IF_CYC_END);
    TMR1_ITCfg(ENABLE, RB_TMR_IE_CYC_END);
    PFIC_EnableIRQ(TMR1_IRQn);
    uint32_t delay_ticks = (ta > PWM_TMR1_ADV_TICKS) ? (ta - PWM_TMR1_ADV_TICKS) : 1;
    // 启动A通道（若有高电平需求）
    if (ta > 0) {
        TMR0_PWMEnable();
        TMR0_Enable();
    }
    TMR1_TimerInit(delay_ticks);
}

// TMR1中断服务程序：在计时Ta结束后开启B通道(PWM8)输出
__INTERRUPT __HIGH_CODE void TMR1_IRQHandler(void)
{
    if (TMR1_GetITFlag(RB_TMR_IF_CYC_END)) {
        // 清除中断标志并停止TMR1
        TMR1_ClearITFlag(RB_TMR_IF_CYC_END);
        TMR1_ITCfg(DISABLE, RB_TMR_IE_CYC_END);
        TMR1_Disable();

        // 读取待触发的PWM8宽度
        uint8_t width        = g_pwm8_width_pending;
        g_pwm8_width_pending = 0;

        if (width) {
            PWMX_CLKCfg(3);
            PWMX_CycleCfg(PWMX_Cycle_256);
            PWMX_ACTOUT(CH_PWM8, width, High_Level, ENABLE);
        }
    }
}

// 使用DelayUs软件延时方式，在A高电平结束后开启B通道(PWM8)
static void PWM_StartHardware_DelayMode(void)
{
    uint32_t ta     = g_ta_ticks;
    uint32_t tb     = g_tb_ticks;
    uint32_t period = g_period_ticks;

    if ((ta == 0) && (tb == 0)) {
        PWM_StopAll();
        return;
    }

    GPIOA_ModeCfg(bTMR0, GPIO_ModeOut_PP_5mA);
    GPIOB_ModeCfg(GPIO_Pin_6, GPIO_ModeOut_PP_5mA);

    PWM_StopAll();

    PRINT("[PWM] StartHW_Delay: period=%lu, ta=%lu, tb=%lu\r\n", period, ta, tb);

    if (ta > 0) {
        TMR0_PWMInit(High_Level, PWM_Times_1);
        TMR0_PWMCycleCfg(period);
        TMR0_PWMActDataWidth(ta);
    }

    uint8_t pwm8_width = 0;
    if (tb > 0) {
        uint32_t tmp = (256U * g_duty2) / 100U;
        if (tmp > 255U) tmp = 255U;
        pwm8_width = (uint8_t)tmp;
    }

    if ((ta == 0) && (tb > 0)) {
        PWMX_CLKCfg(3);
        PWMX_CycleCfg(PWMX_Cycle_256);
        PWMX_ACTOUT(CH_PWM8, pwm8_width, High_Level, ENABLE);
        return;
    }

    if (tb == 0) {
        if (ta > 0) {
            TMR0_PWMEnable();
            TMR0_Enable();
        }
        return;
    }
    PWMX_CLKCfg(3);
    PWMX_CycleCfg(PWMX_Cycle_256);
    if (ta > 0) {
        TMR0_PWMEnable();
        TMR0_Enable();
    }

    {
        volatile uint32_t i;
        for (i = 0; i < ta; i++) {
            __nop();
        }
    }

    PWMX_ACTOUT(CH_PWM8, pwm8_width, High_Level, ENABLE);
}

/**
 * @brief  使用PWMX模块在 PB6 (PWM8) 上启动约80kHz、50%占空比的PWM输出
 */
void PB6_PWMX_80kHz_50Duty_Start(void)
{
    // 目标频率约为 80kHz：F_pwm = FREQ_SYS / (d * N)
    // 这里选择 d = 3, N = 256 -> F_pwm ≈ 60MHz / (3*256) ≈ 78.1kHz

    // 确保PWMX输出在默认引脚：PA12/PA13/PB4/PB6/PB7
    GPIOPinRemap(DISABLE, RB_PIN_PWMX);

    // 配置 PB6 为 PWM8 (PWMX 通道) 输出
    GPIOB_ModeCfg(GPIO_Pin_6, GPIO_ModeOut_PP_5mA);

    // 配置PWMX时钟与周期
    PWMX_CLKCfg(3);                // 基准周期 = 3 / FREQ_SYS
    PWMX_CycleCfg(PWMX_Cycle_256); // 一个PWM周期 = 256 * (3 / FREQ_SYS)

    // 配置PWM8通道输出，高电平占空比约50%
    PWMX_ACTOUT(CH_PWM8, 256 / 2, High_Level, ENABLE); // 128/256 ≈ 50%
}

/*********************************************************************
 * PUBLIC FUNCTIONS
 */

void PWM_ComplementaryInit(void)
{
    PRINT("[PWM] HW PWM Init start (TMR0-PA9, PWM8-PB6)\r\n");

    // 确保TMR0保持在PA9，PWMX保持在默认引脚(PA12/PA13/PB4/PB6/PB7)
    GPIOPinRemap(DISABLE, RB_PIN_TMR0);
    GPIOPinRemap(DISABLE, RB_PIN_PWMX);

    // 配置PA12/PA13为PWM输出引脚（PWM4/PWM5）
    GPIOA_ModeCfg(GPIO_Pin_12 | GPIO_Pin_13, GPIO_ModeOut_PP_5mA);

    // 配置PWMX基准时钟和周期，对应约78kHz
    PWMX_CLKCfg(3);
    PWMX_CycleCfg(PWMX_Cycle_256);

    // 初始化TMR1作为一次性延时定时器（无引脚输出）
    TMR1_Disable();
    TMR1_ITCfg(DISABLE, RB_TMR_IE_CYC_END);
    TMR1_ClearITFlag(RB_TMR_IF_CYC_END);
    PFIC_EnableIRQ(TMR1_IRQn);

    // 初始化内部状态变量
    g_total_duty   = 0;
    g_balance      = 0;
    g_duty1        = 0;
    g_duty2        = 0;
    g_period_ticks = 0;
    g_ta_ticks     = 0;
    g_tb_ticks     = 0;

    // 先关闭所有PWM输出
    PWM_StopAll();

    PRINT("[PWM] Vars & timers reset\r\n");
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
    g_balance    = balance;
    PWM_UpdateOutput();

    // 根据新的占空比参数重新配置PWM4/PWM5输出
    PWM_UpdateHardware_PWMX();

    PRINT("[PWM] Set: Total=%d%%, Balance=%d, duty1=%d%%, duty2=%d%%\r\n",
          g_total_duty, g_balance, g_duty1, g_duty2);
}

void PWM_SetDutyAndBalance_DelayMode(uint8_t total_duty, int8_t balance)
{
    g_total_duty = total_duty;
    g_balance    = balance;
    PWM_UpdateOutput();

    // 延时模式下也直接使用PWMX双通道输出，不再做额外CPU延时
    PWM_UpdateHardware_PWMX();

    PRINT("[PWM][Delay] Set: Total=%d%%, Balance=%d, duty1=%d%%, duty2=%d%%\r\n",
          g_total_duty, g_balance, g_duty1, g_duty2);
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
    if (duty1 != NULL) {
        *duty1 = g_duty1;
    }
    if (duty2 != NULL) {
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
    for (balance = -100; balance <= 100; balance += 25) {
        PWM_SetDutyAndBalance(total_duty, balance);
        // PWM_SetDutyAndBalance_DelayMode(total_duty, balance);
        PWM_GetActualDuty(&duty1, &duty2);
        PRINT("%d\t%d\t%d\t%d\r\n", balance, duty1, duty2, duty1 + duty2);

        // 延时，方便观察波形
        DelayMs(5000);
    }

    // // 测试2: 固定balance=0，调整总占空比从0到100%
    // PRINT("\r\n[Test 2] Balance = 0 (balanced), Total Duty varies\r\n");
    // PRINT("Total%%\tPWM1%%\tPWM2%%\tSum%%\r\n");
    // PRINT("------\t-----\t-----\t----\r\n");

    // balance = 0;
    // for(total_duty = 0; total_duty <= 100; total_duty += 20)
    // {
    //     PWM_SetDutyAndBalance(total_duty, balance);
    //     PWM_GetActualDuty(&duty1, &duty2);
    //     PRINT("%d\t%d\t%d\t%d\r\n", total_duty, duty1, duty2, duty1 + duty2);

    //     // 延时，方便观察波形
    //     DelayMs(1000);
    // }

    // // 测试3: 不同总占空比和balance的组合
    // PRINT("\r\n[Test 3] Various combinations\r\n");
    // PRINT("Total%%\tBalance\tPWM1%%\tPWM2%%\tSum%%\r\n");
    // PRINT("------\t-------\t-----\t-----\t----\r\n");

    // // 组合1: 30% duty, +50 balance
    // PWM_SetDutyAndBalance(30, 50);
    // PWM_GetActualDuty(&duty1, &duty2);
    // PRINT("%d\t%d\t%d\t%d\t%d\r\n", 30, 50, duty1, duty2, duty1 + duty2);
    // DelayMs(100);

    // // 组合2: 80% duty, -40 balance
    // PWM_SetDutyAndBalance(80, -40);
    // PWM_GetActualDuty(&duty1, &duty2);
    // PRINT("%d\t%d\t%d\t%d\t%d\r\n", 80, -40, duty1, duty2, duty1 + duty2);
    // DelayMs(100);

    // // 组合3: 100% duty, +100 balance (全部给PWM1)
    // PWM_SetDutyAndBalance(100, 100);
    // PWM_GetActualDuty(&duty1, &duty2);
    // PRINT("%d\t%d\t%d\t%d\t%d\r\n", 100, 100, duty1, duty2, duty1 + duty2);
    // DelayMs(100);

    // // 组合4: 100% duty, -100 balance (全部给PWM2)
    // PWM_SetDutyAndBalance(100, -100);
    // PWM_GetActualDuty(&duty1, &duty2);
    // PRINT("%d\t%d\t%d\t%d\t%d\r\n", 100, -100, duty1, duty2, duty1 + duty2);
    // DelayMs(100);

    // // 测试完成，恢复到默认状态
    // PWM_SetDutyAndBalance(50, 0);
    // PWM_GetActualDuty(&duty1, &duty2);

    PRINT("\r\n[Test Complete] Reset to: Total=50%%, Balance=0\r\n");
    PRINT("Final state: PWM1=%d%%, PWM2=%d%%\r\n", duty1, duty2);
    PRINT("====================================================\r\n\r\n");
}

/**
 * @brief  在 PB22 上启动 TMR3 PWM，80kHz，50% 占空比
 */
void PB22_PWM_80kHz_50Duty_Start(void)
{
    uint32_t period;

    // 目标频率 80kHz：period = FREQ_SYS / 80000
    period = FREQ_SYS / 80000;
    if (period == 0)
        period = 1;

    // 将 TMR3/PWM3 重映射到 PB22
    GPIOPinRemap(ENABLE, RB_PIN_TMR3);

    // 配置 PB22 为 TMR3/PWM3 输出
    GPIOB_ModeCfg(bTMR3, GPIO_ModeOut_PP_5mA);

    // 先关掉 TMR3 PWM 和计数器
    TMR3_PWMDisable();
    TMR3_Disable();

    // 初始化 TMR3 PWM：高电平有效，连续输出
    TMR3_PWMInit(High_Level, PWM_Times_1);

    // 设置周期和占空比（50%）
    TMR3_PWMCycleCfg(period);
    TMR3_PWMActDataWidth(period / 2);

    // 启动 TMR3 PWM 输出
    TMR3_PWMEnable();
    TMR3_Enable();
}
