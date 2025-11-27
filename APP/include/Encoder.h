#ifndef __ENCODER_H__
#define __ENCODER_H__

#include <stdbool.h>
#include "CH58x_common.h"

// 硬件引脚定义
// PA9 - Encoder A
// PA8 - Encoder B
// PB0 - Encoder SW (按钮)
#define ENCODER_A_PIN       GPIO_Pin_9
#define ENCODER_B_PIN       GPIO_Pin_8
#define ENCODER_SW_PIN      GPIO_Pin_0

// 编码器旋转方向
typedef enum {
    ENCODER_DIR_NONE = 0,    // 无旋转
    ENCODER_DIR_CW = 1,      // 顺时针
    ENCODER_DIR_CCW = -1     // 逆时针
} Encoder_Direction;

// 编码器速度等级
typedef enum {
    ENCODER_SPEED_STOP = 0,  // 停止
    ENCODER_SPEED_SLOW,      // 慢速
    ENCODER_SPEED_MEDIUM,    // 中速
    ENCODER_SPEED_FAST       // 快速
} Encoder_Speed;

// 编码器状态结构体
typedef struct {
    int32_t count;           // 累计计数值（正数=顺时针，负数=逆时针）
    Encoder_Direction dir;   // 当前旋转方向
    Encoder_Speed speed;     // 当前旋转速度等级
    uint32_t rpm;            // 估算转速（脉冲/秒）
    bool sw_pressed;         // 按钮是否按下
    uint32_t last_update;    // 上次更新时间（用于速度计算）
} Encoder_Status;

// 函数声明
void Encoder_Init(void);
void Encoder_IRQHandler(void);
int32_t Encoder_GetCount(void);
void Encoder_ResetCount(void);
Encoder_Direction Encoder_GetDirection(void);
Encoder_Speed Encoder_GetSpeed(void);
uint32_t Encoder_GetRPM(void);
bool Encoder_IsButtonPressed(void);
void Encoder_GetStatus(Encoder_Status *status);
void Encoder_Test(void);

#endif // __ENCODER_H__