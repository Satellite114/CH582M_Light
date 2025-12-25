/********************************** (C) COPYRIGHT *******************************
 * File Name          : PWM.h
 * Author             : 
 * Version            : V2.0
 * Date               : 2025/12/01
 * Description        : 互补PWM控制模块头文件（基于TMR0定时器中断实现）
 *                      实现真正的互补PWM输出（不重叠，顺序输出）
 *                      - PWM频率：约80kHz (实际78.43kHz)
 *                      - 分辨率：100步
 *                      - PWM1在0到duty1时间段输出高电平
 *                      - PWM2在duty1到duty1+duty2时间段输出高电平
 *                      - 总占空比可调，两个PWM之间的分配比例可调
 *******************************************************************************/

#ifndef __PWM_H__
#define __PWM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "CH58x_common.h"

/**
 * @brief  PWM通道定义
 */
#define PWM_CHANNEL_1    CH_PWM4    // PWM通道1使用PWM4
#define PWM_CHANNEL_2    CH_PWM5    // PWM通道2使用PWM5

/**
 * @brief  PWM周期定义
 */
#define PWM_CYCLE_MAX    256        // PWM周期最大值

/**
 * @brief  初始化互补PWM控制
 *         使用PWM4和PWM5作为两个独立可调的PWM输出
 *         
 * @return  None
 */
void PWM_ComplementaryInit(void);

/**
 * @brief  设置总占空比
 *         两个PWM的占空比之和等于总占空比
 *         
 * @param  duty - 总占空比，范围0-100 (%)
 * 
 * @return  None
 */
void PWM_SetTotalDuty(uint8_t duty);

/**
 * @brief  设置平衡度
 *         控制两个PWM之间的分配关系
 *         balance = 0: 完全平衡，duty1 = duty2 = total_duty/2
 *         balance > 0: PWM1增大，PWM2减小
 *         balance < 0: PWM1减小，PWM2增大
 *         
 * @param  balance - 平衡度，范围-100到+100
 *                   +100: PWM1=total_duty, PWM2=0
 *                   -100: PWM1=0, PWM2=total_duty
 * 
 * @return  None
 */
void PWM_SetBalance(int8_t balance);

/**
 * @brief  同时设置总占空比和平衡度
 *         
 * @param  total_duty - 总占空比，范围0-100 (%)
 * @param  balance    - 平衡度，范围-100到+100
 * 
 * @return  None
 */
void PWM_SetDutyAndBalance(uint8_t total_duty, int8_t balance);

// 使用DelayUs软件延时方式，在A高电平结束后开启B通道的版本
void PWM_SetDutyAndBalance_DelayMode(uint8_t total_duty, int8_t balance);

/**
 * @brief  获取当前PWM1和PWM2的实际占空比
 *         
 * @param  duty1 - 输出PWM1的占空比 (0-100%)
 * @param  duty2 - 输出PWM2的占空比 (0-100%)
 * 
 * @return  None
 */
void PWM_GetActualDuty(uint8_t *duty1, uint8_t *duty2);

/**
 * @brief  PWM测试函数
 *         测试不同总占空比和平衡度组合下的PWM输出
 *         通过串口输出测试结果
 *         
 * @return  None
 */
void PWM_Test(void);

// 使用PWMX模块在 PB6 (PWM8) 上输出约80kHz、50%占空比PWM的最小测试函数
void PB6_PWMX_80kHz_50Duty_Start(void);

#ifdef __cplusplus
}
#endif

#endif // __PWM_H__
