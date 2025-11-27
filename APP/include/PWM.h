#ifndef __PWM_H__
#define __PWM_H__

#include <stdbool.h>
#include "CH58x_common.h"
#include "CH58x_pwm.h"

// PA12 - PWM4
// PA13 - PWM5
// PB0 - PWM6
// PB4 - PWM7
// PB6 - PWM8
// PB7 - PWM9
// PB14 - PWM10

typedef enum {
    PWM_COMPLEMENTARY_PAIR_4_5 = 0,
    PWM_COMPLEMENTARY_PAIR_6_7,
    PWM_COMPLEMENTARY_PAIR_8_9,
    PWM_COMPLEMENTARY_PAIR_10_11,
    PWM_COMPLEMENTARY_PAIR_MAX
} PWM_ComplementaryPair;

void PWM_ComplementaryInit(PWM_ComplementaryPair pair, uint32_t freqHz, float dutyPercent);
void PWM_ComplementarySetFrequency(uint32_t freqHz);
void PWM_ComplementarySetDuty(PWM_ComplementaryPair pair, float dutyPercent);
void PWM_ComplementaryEnable(PWM_ComplementaryPair pair, bool enable);
uint32_t PWM_ComplementaryGetActualFrequency(void);
float PWM_ComplementaryGetDuty(PWM_ComplementaryPair pair);
void PWM_Test(void);

#endif // __PWM_H__