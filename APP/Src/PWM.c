#include <stddef.h>
#include <stdint.h>
#include "PWM.h"
#include "CH58x_gpio.h"

typedef struct
{
    PWM_ComplementaryPair id;
    uint8_t channelA;
    uint8_t channelB;
    uint8_t staggerMask;
} PWM_ChannelPairDesc;

typedef struct
{
    PWMX_CycleTypeDef cfg;
    uint16_t length;
} PWM_CycleOption;

static const PWM_ChannelPairDesc kPairDesc[PWM_COMPLEMENTARY_PAIR_MAX] = {
    {PWM_COMPLEMENTARY_PAIR_4_5, CH_PWM4, CH_PWM5, RB_PWM4_5_STAG_EN},
    {PWM_COMPLEMENTARY_PAIR_6_7, CH_PWM6, CH_PWM7, RB_PWM6_7_STAG_EN},
    {PWM_COMPLEMENTARY_PAIR_8_9, CH_PWM8, CH_PWM9, RB_PWM8_9_STAG_EN},
    {PWM_COMPLEMENTARY_PAIR_10_11, CH_PWM10, CH_PWM11, RB_PWM10_11_STAG_EN},
};

static const PWM_CycleOption kCycleOptions[] = {
    {PWMX_Cycle_256, 256},
    {PWMX_Cycle_255, 255},
    {PWMX_Cycle_128, 128},
    {PWMX_Cycle_127, 127},
    {PWMX_Cycle_64, 64},
    {PWMX_Cycle_63, 63},
    {PWMX_Cycle_32, 32},
    {PWMX_Cycle_31, 31},
};

static uint8_t s_clockDiv = 4;
static PWMX_CycleTypeDef s_cycleCfg = PWMX_Cycle_64;
static uint16_t s_cycleLen = 64;
static uint32_t s_actualFreqHz = 0;
static float s_pairDutyPercent[PWM_COMPLEMENTARY_PAIR_MAX] = {0};
static bool s_pairInitialized[PWM_COMPLEMENTARY_PAIR_MAX] = {false};
static bool s_pairEnabled[PWM_COMPLEMENTARY_PAIR_MAX] = {false};
static bool s_frequencyConfigured = false;

static const PWM_ChannelPairDesc *PWM_GetPairDesc(PWM_ComplementaryPair pair)
{
    if(pair >= PWM_COMPLEMENTARY_PAIR_MAX)
    {
        return NULL;
    }
    return &kPairDesc[pair];
}

static void PWM_WriteDataRegister(uint8_t channelMask, uint8_t ticks)
{
    volatile uint8_t *dataBase = &R8_PWM4_DATA;
    for(uint8_t bit = 0; bit < 8; bit++)
    {
        if(channelMask & (1u << bit))
        {
            dataBase[bit] = ticks;
        }
    }
}

static uint8_t PWM_DutyPercentToTicks(float dutyPercent)
{
    float clamped = dutyPercent;
    if(clamped < 0.0f)
    {
        clamped = 0.0f;
    }
    if(clamped > 100.0f)
    {
        clamped = 100.0f;
    }

    uint16_t maxCount = (s_cycleLen == 256) ? 255 : s_cycleLen;
    uint16_t ticks = (uint16_t)((clamped * (float)maxCount / 100.0f) + 0.5f);
    if(ticks > maxCount)
    {
        ticks = maxCount;
    }
    return (uint8_t)ticks;
}

static void PWM_ReapplyPair(const PWM_ChannelPairDesc *desc, PWM_ComplementaryPair pair)
{
    uint8_t ticks = PWM_DutyPercentToTicks(s_pairDutyPercent[pair]);
    if(s_pairEnabled[pair])
    {
        PWMX_ACTOUT(desc->channelA, ticks, Low_Level, ENABLE);
        PWMX_ACTOUT(desc->channelB, ticks, High_Level, ENABLE);
    }
    else
    {
        PWM_WriteDataRegister(desc->channelA, ticks);
        PWM_WriteDataRegister(desc->channelB, ticks);
    }
}

void PWM_ComplementarySetFrequency(uint32_t freqHz)
{
    if(freqHz == 0)
    {
        freqHz = 1;
    }

    uint32_t sysClk = GetSysClock();
    uint32_t bestError = UINT32_MAX;
    uint8_t bestDiv = s_clockDiv;
    PWMX_CycleTypeDef bestCycle = s_cycleCfg;
    uint16_t bestLen = s_cycleLen;

    for(size_t i = 0; i < sizeof(kCycleOptions) / sizeof(kCycleOptions[0]); i++)
    {
        const PWM_CycleOption *opt = &kCycleOptions[i];
        uint64_t denominator = (uint64_t)freqHz * opt->length;
        if(denominator == 0)
        {
            continue;
        }

        uint64_t div = ((uint64_t)sysClk + (denominator / 2)) / denominator;
        if(div == 0)
        {
            div = 1;
        }
        if(div > 255)
        {
            div = 255;
        }

        uint32_t actualFreq = sysClk / (uint32_t)(div * opt->length);
        uint32_t error = (actualFreq > freqHz) ? (actualFreq - freqHz) : (freqHz - actualFreq);

        if(error < bestError)
        {
            bestError = error;
            bestDiv = (uint8_t)div;
            bestCycle = opt->cfg;
            bestLen = opt->length;

            if(error == 0)
            {
                break;
            }
        }
    }

    s_clockDiv = bestDiv;
    s_cycleCfg = bestCycle;
    s_cycleLen = bestLen;
    s_actualFreqHz = sysClk / (bestDiv * bestLen);
    s_frequencyConfigured = true;

    PWMX_CLKCfg(s_clockDiv);
    PWMX_CycleCfg(s_cycleCfg);

    for(PWM_ComplementaryPair pair = 0; pair < PWM_COMPLEMENTARY_PAIR_MAX; pair++)
    {
        if(s_pairInitialized[pair])
        {
            const PWM_ChannelPairDesc *desc = PWM_GetPairDesc(pair);
            if(desc != NULL)
            {
                PWM_ReapplyPair(desc, pair);
            }
        }
    }
}

void PWM_ComplementarySetDuty(PWM_ComplementaryPair pair, float dutyPercent)
{
    const PWM_ChannelPairDesc *desc = PWM_GetPairDesc(pair);
    if(desc == NULL)
    {
        return;
    }

    if(!s_frequencyConfigured)
    {
        PWM_ComplementarySetFrequency(s_actualFreqHz ? s_actualFreqHz : 10000);
    }

    s_pairDutyPercent[pair] = dutyPercent;
    uint8_t ticks = PWM_DutyPercentToTicks(dutyPercent);

    if(s_pairEnabled[pair])
    {
        PWMX_ACTOUT(desc->channelA, ticks, Low_Level, ENABLE);
        PWMX_ACTOUT(desc->channelB, ticks, High_Level, ENABLE);
    }
    else
    {
        PWM_WriteDataRegister(desc->channelA, ticks);
        PWM_WriteDataRegister(desc->channelB, ticks);
    }
}

void PWM_ComplementaryEnable(PWM_ComplementaryPair pair, bool enable)
{
    const PWM_ChannelPairDesc *desc = PWM_GetPairDesc(pair);
    if(desc == NULL || !s_pairInitialized[pair])
    {
        return;
    }

    uint8_t ticks = PWM_DutyPercentToTicks(s_pairDutyPercent[pair]);

    if(enable)
    {
        PWMX_ACTOUT(desc->channelA, ticks, Low_Level, ENABLE);
        PWMX_ACTOUT(desc->channelB, ticks, High_Level, ENABLE);
        s_pairEnabled[pair] = true;
    }
    else
    {
        PWMX_ACTOUT(desc->channelA, 0, Low_Level, DISABLE);
        PWMX_ACTOUT(desc->channelB, 0, High_Level, DISABLE);
        s_pairEnabled[pair] = false;
    }
}

void PWM_ComplementaryInit(PWM_ComplementaryPair pair, uint32_t freqHz, float dutyPercent)
{
    const PWM_ChannelPairDesc *desc = PWM_GetPairDesc(pair);
    if(desc == NULL)
    {
        return;
    }

    if(!s_frequencyConfigured)
    {
        s_actualFreqHz = freqHz;
    }

    PWMX_AlterOutCfg(desc->staggerMask, DISABLE);
    PWM_ComplementarySetFrequency(freqHz);
    s_pairInitialized[pair] = true;
    s_pairEnabled[pair] = false;
    PWM_ComplementarySetDuty(pair, dutyPercent);
}

uint32_t PWM_ComplementaryGetActualFrequency(void)
{
    return s_actualFreqHz;
}

float PWM_ComplementaryGetDuty(PWM_ComplementaryPair pair)
{
    if(pair >= PWM_COMPLEMENTARY_PAIR_MAX)
    {
        return 0.0f;
    }
    return s_pairDutyPercent[pair];
}

void PWM_Test(void)
{
    GPIOB_ModeCfg(GPIO_Pin_6, GPIO_ModeOut_PP_5mA);
    GPIOB_ModeCfg(GPIO_Pin_7, GPIO_ModeOut_PP_5mA);

    PWM_ComplementaryInit(PWM_COMPLEMENTARY_PAIR_8_9, 80000, 40.0f);
    PWM_ComplementaryEnable(PWM_COMPLEMENTARY_PAIR_8_9, true);

    for(uint8_t i = 0; i < 6; i++)
    {
        float duty = 20.0f + (i * 10.0f);
        if(duty > 80.0f)
        {
            duty = 80.0f;
        }
        PWM_ComplementarySetDuty(PWM_COMPLEMENTARY_PAIR_8_9, duty);
        mDelaymS(200);
    }

    PWM_ComplementarySetDuty(PWM_COMPLEMENTARY_PAIR_8_9, 50.0f);
}