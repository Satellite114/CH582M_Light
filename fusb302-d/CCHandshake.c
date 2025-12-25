/**
 * usbc-pd-fusb302-d: Library for ONSEMI FUSB302-D (USB-C Controller) for PD negotiation
 * Copyright (C) 2020  Philip Tschiemer https://filou.se
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "CCHandshake.h"
#include "PD.h"
#include "util-string.h"
#include "util-hex.h"


#include "stdio.h"
static FUSB302_D_t Driver;
uint8_t fusb302_event = 0;
#define PD_REQUEST_TIMEOUT_MS 1000
#define PD_MAX_RETRY_COUNT 3
#if ONSEMI_LIBRARY == true

#include "Platform_ARM/app/HWIO.h"
#include "Platform_ARM/app/TimeDelay.h"
#include "Platform_ARM/app/Timing.h"
#include "Platform_ARM/app/PlatformI2C.h"
// #include "core/TypeC.h"

#ifdef PWR
#undef PWR
#endif
#ifdef COMP
#undef COMP
#endif

#include "core/fusb30X.h"

extern DeviceReg_t Registers;

#else

// #include "main.h"

#include <string.h>
#include "util-string.h" // https://github.com/tschiemer/c-utils

typedef enum
{
    PD_State_Disabled,
    PD_State_HardReset,
    PD_State_Reset,
    PD_State_Idle,
    PD_State_Rx,
    PD_State_AwaitGoodCRC,
    PD_State_Resend,
} PD_State_t;

// static uint8_t * regPtr( FUSB302_D_Register_t reg );

static uint8_t read(FUSB302_D_Register_t reg, uint8_t *value);
static uint8_t write(FUSB302_D_Register_t reg, uint8_t value);

static uint8_t readAll(void);
static uint8_t readStatus(void);
static uint8_t configure(void);

static void typeC_core(void);

static uint8_t readCCVoltageLevel(uint8_t *bc_lvl);
static CCHandshake_CC_t detectCCPinSink(void);
static uint8_t enableSink(CCHandshake_CC_t cc);
static uint8_t disableSink();

static void pd_core(void);

static void pd_init(void);
static void pd_deinit(void);

static uint8_t pd_hasMessage(void);
static uint8_t pd_getMessage(PD_Message_t *message);
static uint8_t pd_sendMessage(PD_Message_t *message, PD_State_t (*onAcknowledged)(PD_Message_t *message));

static PD_State_t pd_processMessage(PD_Message_t *message);
static PD_State_t pd_onSourceCapabilities(PD_Message_t *message);

static void pd_createRequest(PD_Message_t *message);

static void pd_hardreset(void);
static void pd_reset(void);

// static void pd_setRoles( CCHandshake_PD_Role_t powerRole, CCHandshake_PD_Role_t dataRole )
// static void pd_setAutoGoodCrc( uint8_t enabled );

static void pd_flushRxFifo(void);
static void pd_flushTxFifo(void);

static void pd_startTx(void);

static FUSB302_D_Registers_st Registers;

static volatile CCHandshake_CC_t ConnectedCC;

static struct
{
    volatile PD_State_t State;
    struct
    {
        uint8_t MessageId;
        PD_Message_t Message;
        uint8_t HasData;
    } Rx;
    struct
    {
        uint8_t MessageId;
        PD_Message_t Message;
        PD_State_t (*OnAcknowledged)(PD_Message_t *message);
        TimerTime_t SentTs;
        uint8_t SendAttempts;
        uint8_t RetryCount; // 添加重试计数
    } Tx;
    struct
    {
        uint8_t NSourceCapabilities;
        PD_DataObject_t SourceCapabilities[PD_MESSAGE_MAX_OBJECTS];
        uint8_t BestCapIndex;
    } Power;
} PD;
static inline void pd_clearTx(void)
{
    memset((uint8_t *)&PD.Tx.Message, 0, sizeof(PD_Message_t));
}

static inline uint8_t pd_nextTxMessageId(void)
{
    //	uint8_t mid = PD.Rx.MessageId + 1;
    uint8_t mid = PD.Tx.MessageId;

    PD.Tx.MessageId = (PD.Tx.MessageId + 1) % PD_MESSAGE_MAX_MID;

    return mid;
}

#endif

void CCHandshake_init(void)
{
    FUSB302_D_Init(&Driver, FUSB302_D_DEFAULT_ADDRESS);
    volatile uint8_t id = 0;

    // 调用你已有的寄存器读取函数
    if (FUSB302_D_Read(&Driver, 0x01, &id) != FUSB302_D_OK)
    {
        // while (1)
        //     ;
        printf("error_handle\n");
    }

    if (FUSB302_D_Probe(&Driver, 3, 100) == FUSB302_D_ERROR)
    {
        printf("error_handle\n");
        while(1);
        printf("error!\r\n");
    }

#if ONSEMI_LIBRARY == true

    //    InitializeBoard();
    //    InitializeDelay();

    InitializeCoreTimer();

    core_initialize();

    printf("CC pid=%d v=%d rev=%d\n", Registers.DeviceID.PRODUCT_ID, Registers.DeviceID.VERSION_ID, Registers.DeviceID.REVISION_ID);

    core_enable_typec(TRUE); // Enable the state machine by default

    //	SetStateUnattached();

#else
    ConnectedCC = CCHandshake_CC_None;

    //	read( FUSB302_D_Register_Reset,  );
    //	Registers.Reset |= FUSB302_D_Reset_SW_RES;
    write(FUSB302_D_Register_Reset, FUSB302_D_Reset_SW_RES);
    //	Registers.Reset &= ~FUSB302_D_Reset_SW_RES;

    if (readAll() == false)
    {
        printf("error_handle\n");
        while(1);
        printf("error!\r\n");
    }
    if (configure() == false)
    {
        printf("error_handle\n");
        while(1);
        printf("error!\r\n");
    }

    PD.State = PD_State_Disabled;
    printf("PD.State = %d\n", PD.State);

#endif
}

void CCHandshake_deinit(void)
{
#if ONSEMI_LIBRARY == false
    ConnectedCC = CCHandshake_CC_None;
    PD.State = PD_State_Disabled;
#endif
}

CCHandshake_CC_t CCHandshake_getOrientation(void)
{
#if ONSEMI_LIBRARY == true
    return CCHandshake_CC_None;
#else
    return ConnectedCC;
#endif
}
void CCHandshake_core(void)
{
#if ONSEMI_LIBRARY == true
    core_state_machine();
#else
#if CCHANDSHAKE_AUTONOMOUS == true

#else
    // 轮询检查中断状态
    if (readStatus() == false)
    {
        return; // 读取状态失败，退出
    }

    // 检查是否有中断标志
    if (Registers.Interrupt != 0 ||
        Registers.Interrupta != 0 ||
        Registers.Interruptb != 0)
    {
        printf("Interrupt  %02x %02x %02x\n",
            Registers.Interrupta,
            Registers.Interruptb,
            Registers.Interrupt);

        typeC_core();
        pd_core();
    }
    else
    {
        // 没有中断，检查PD状态机是否需要处理
        if (PD.State != PD_State_Disabled)
        {
            pd_core();
        }
    }
#endif
#endif /* ONSEMI_LIBRARY */
}
#if ONSEMI_LIBRARY == true

FSC_uint8_t platform_get_device_irq_state(void)
{
    uint8_t reg = 0;

    DeviceRead(regInterrupta, 2, &Registers.Status.InterruptAdv);
    DeviceRead(regInterrupt, 1, &Registers.Status.Interrupt1);

    if (Registers.Status.InterruptAdv != 0 || Registers.Status.Interrupt1 != 0)
    {
        //		printf("interruptAdv %04X \t interrupt1 %04X\n", Registers.Status.InterruptAdv, Registers.Status.Interrupt1 );
        if (Registers.Status.I_OCP_TEMP)
        {
            printf("I_OCP_TEMP ");
        }
        if (Registers.Status.I_TOGDONE)
        {
            printf("I_TOGDONE ");
        }
        if (Registers.Status.I_SOFTFAIL)
        {
            printf("I_SOFTFAIL ");
        }
        if (Registers.Status.I_RETRYFAIL)
        {
            printf("I_RETRYFAIL ");
        }
        if (Registers.Status.I_HARDSENT)
        {
            printf("I_HARDSENT ");
        }
        if (Registers.Status.I_TXSENT)
        {
            printf("I_TXSENT ");
        }
        if (Registers.Status.I_SOFTRST)
        {
            printf("I_SOFTRST ");
        }
        if (Registers.Status.I_HARDRST)
        {
            printf("I_HARDRST ");
        }

        if (Registers.Status.I_GCRCSENT)
        {
            printf("I_GCRCSENT ");
        }

        if (Registers.Status.I_VBUSOK)
        {
            printf("I_VBUSOK ");
        }
        if (Registers.Status.I_ACTIVITY)
        {
            printf("I_ACTIVITY ");
        }
        if (Registers.Status.I_COMP_CHNG)
        {
            printf("I_COMP_CHNG ");
        }
        if (Registers.Status.I_CRC_CHK)
        {
            printf("I_CRC_CHK ");
        }
        if (Registers.Status.I_ALERT)
        {
            printf("I_ALERT ");
        }
        if (Registers.Status.I_WAKE)
        {
            printf("I_WAKE ");
        }
        if (Registers.Status.I_COLLISION)
        {
            printf("I_COLLISION ");
        }
        if (Registers.Status.I_BC_LVL)
        {
            printf("I_BC_LVL ");
        }

        printf("\n");
    }

    return (Registers.Status.Interrupt1 != 0 || Registers.Status.InterruptAdv != 0);
    //
    //	if (FUSB302_D_Read( &Driver, FUSB302_D_Register_Interrupta, &reg) == FUSB302_D_OK)
    //	{
    //		if (reg != interrupta)
    //		{
    //			interrupta = reg;
    //			return TRUE;
    //		}
    //	}
    //
    //	if (FUSB302_D_Read( &Driver, FUSB302_D_Register_Interruptb, &reg) == FUSB302_D_OK)
    //	{
    //		if (reg != interruptb)
    //		{
    //			interruptb = reg;
    //			return TRUE;
    //		}
    //	}
    //
    //	if (FUSB302_D_Read( &Driver, FUSB302_D_Register_Status0, &reg) == FUSB302_D_OK)
    //	{
    //		if (reg != status0)
    //		{
    //			status0 = reg;
    //			return TRUE;
    //		}
    //	}
    //
    //	if (FUSB302_D_Read( &Driver, FUSB302_D_Register_Status1, &reg) == FUSB302_D_OK)
    //	{
    //		if (reg != status1)
    //		{
    //			status1 = reg;
    //			return TRUE;
    //		}
    //	}
    //
    //	if (FUSB302_D_Read( &Driver, FUSB302_D_Register_Interrupt, &reg) == FUSB302_D_OK)
    //	{
    //		if (reg != interrupt)
    //		{
    //			interrupt = reg;
    //			return TRUE;
    //		}
    //	}
}

#else
static void typeC_core(void)
{
    // 如果未连接，检查是否有连接
    if (ConnectedCC == CCHandshake_CC_None)
    {
        CCHandshake_CC_t detected = detectCCPinSink();

        if (detected == CCHandshake_CC_None)
        {
            return;
        }

        if (enableSink(detected) == false)
        {
            return;
        }

        pd_init();
        ConnectedCC = detected;
        printf("CC detected %d\n", detected);
    }
    else // 检查是否仍然连接
    {
        // 检查BC_LVL变化中断
        if ((Registers.Interrupt & FUSB302_D_Interrupt_I_BC_LVL) == FUSB302_D_Interrupt_I_BC_LVL)
        {
            uint8_t bc_lvl = 0;
            if (readCCVoltageLevel(&bc_lvl) == false)
            {
                return;
            }

            // 检查是否仍然在阈值以上
            if (bc_lvl >= CCHANDSHAKE_REQUIRE_BC_LVL)
            {
                return; // 仍然连接
            }

            // 连接丢失
            if (disableSink() == false)
            {
                // 处理错误
            }

            pd_deinit();
            ConnectedCC = CCHandshake_CC_None;
            printf("CC lost\n");
        }
    }
}

// inline static uint8_t * regPtr( FUSB302_D_Register_t reg )
//{
//	switch (reg){
//		case FUSB302_D_Register_DeviceID: 	return &Registers.DeviceID;
//		case FUSB302_D_Register_Switches0: 	return &Registers.Switches0;
//		case FUSB302_D_Register_Switches1: 	return &Registers.Switches1;
//		case FUSB302_D_Register_Measure: 	return &Registers.Measure;
//		case FUSB302_D_Register_Slice: 		return &Registers.Slice;
//		case FUSB302_D_Register_Control0: 	return &Registers.Control0;
//		case FUSB302_D_Register_Control1: 	return &Registers.Control1;
//		case FUSB302_D_Register_Control2: 	return &Registers.Control2;
//		case FUSB302_D_Register_Control3: 	return &Registers.Control3;
//		case FUSB302_D_Register_Mask1: 		return &Registers.Mask1;
//		case FUSB302_D_Register_Power: 		return &Registers.Power;
//		case FUSB302_D_Register_Reset: 		return &Registers.Reset;
//		case FUSB302_D_Register_OCPreg: 	return &Registers.OCPreg;
//		case FUSB302_D_Register_Maska: 		return &Registers.Maska;
//		case FUSB302_D_Register_Maskb: 		return &Registers.Maskb;
//		case FUSB302_D_Register_Control4: 	return &Registers.Control4;
//		case FUSB302_D_Register_Status0a: 	return &Registers.Status0a;
//		case FUSB302_D_Register_Status1a: 	return &Registers.Status1a;
//		case FUSB302_D_Register_Interrupta: return &Registers.Interrupta;
//		case FUSB302_D_Register_Interruptb: return &Registers.Interruptb;
//		case FUSB302_D_Register_Status0: 	return &Registers.Status0;
//		case FUSB302_D_Register_Status1: 	return &Registers.Status1;
//		case FUSB302_D_Register_Interrupt: 	return &Registers.Interrupt;
//		case FUSB302_D_Register_FIFOs: 		return &Registers.FIFOs;
//
//		default: return NULL;
//	}
// }

static uint8_t read(FUSB302_D_Register_t reg, uint8_t *value)
{
    //	uint8_t * r = regPtr( reg );
    //
    //	if (r == NULL)
    //	{
    //		return false;
    //	}

    uint8_t r[2] = {0, 0};

    if (FUSB302_D_Read(&Driver, reg, &r[0]) == FUSB302_D_ERROR)
    {
        return false;
    }

    *value = r[0];

    return true;
}

static uint8_t write(FUSB302_D_Register_t reg, uint8_t value)
{
    //	uint8_t * r = regPtr( reg );
    //
    //	if (r == NULL)
    //	{
    //		return false;
    //	}

    if (FUSB302_D_Write(&Driver, reg, value) == FUSB302_D_ERROR)
    {
        return false;
    }

    return true;
}

static uint8_t readAll(void)
{
    if (read(FUSB302_D_Register_DeviceID, &Registers.DeviceID) == false)
        return false;
    if (read(FUSB302_D_Register_Switches0, &Registers.Switches0) == false)
        return false;
    if (read(FUSB302_D_Register_Switches1, &Registers.Switches1) == false)
        return false;
    if (read(FUSB302_D_Register_Measure, &Registers.Measure) == false)
        return false;
    if (read(FUSB302_D_Register_Slice, &Registers.Slice) == false)
        return false;
    if (read(FUSB302_D_Register_Control0, &Registers.Control0) == false)
        return false;
    if (read(FUSB302_D_Register_Control1, &Registers.Control1) == false)
        return false;
    if (read(FUSB302_D_Register_Control2, &Registers.Control2) == false)
        return false;
    if (read(FUSB302_D_Register_Control3, &Registers.Control3) == false)
        return false;
    if (read(FUSB302_D_Register_Mask1, &Registers.Mask1) == false)
        return false;
    if (read(FUSB302_D_Register_Power, &Registers.Power) == false)
        return false;
    //	if (read(FUSB302_D_Register_Reset, &Registers.Reset) == false) return false; // no point in reading this
    if (read(FUSB302_D_Register_OCPreg, &Registers.OCPreg) == false)
        return false;
    if (read(FUSB302_D_Register_Maska, &Registers.Maska) == false)
        return false;
    if (read(FUSB302_D_Register_Maskb, &Registers.Maskb) == false)
        return false;
    if (read(FUSB302_D_Register_Control4, &Registers.Control4) == false)
        return false;
    if (read(FUSB302_D_Register_Status0a, &Registers.Status0a) == false)
        return false;
    if (read(FUSB302_D_Register_Status1a, &Registers.Status1a) == false)
        return false;
    if (read(FUSB302_D_Register_Status0, &Registers.Status0) == false)
        return false;
    if (read(FUSB302_D_Register_Status1, &Registers.Status1) == false)
        return false;
    if (read(FUSB302_D_Register_FIFOs, &Registers.FIFOs) == false)
        return false;
    return true;
}

static uint8_t readStatus(void)
{
    uint8_t buf[5] = {0, 0, 0, 0, 0};

    if (FUSB302_D_ReadN(&Driver, FUSB302_D_Register_Interrupta, &buf[0], 5) == FUSB302_D_ERROR)
    {
        printf("failed read\n");
        return false;
    }
    Registers.Interrupta = buf[0];
    Registers.Interruptb = buf[1];
    Registers.Status0 = buf[2];
    Registers.Status1 = buf[3];
    Registers.Interrupt = buf[4];

    //	if (read(FUSB302_D_Register_Interrupta, &Registers.Interrupta) == false) return false;
    //	if (read(FUSB302_D_Register_Interruptb, &Registers.Interruptb) == false) return false;
    //	if (read(FUSB302_D_Register_Status0, &Registers.Status0) == false) return false;
    //	if (read(FUSB302_D_Register_Status1, &Registers.Status1) == false) return false;
    //	if (read(FUSB302_D_Register_Interrupt, &Registers.Interrupt) == false) return false;

    return true;
}

static uint8_t configure(void)
{
#if CCHANDSHAKE_AUTONOMOUS == true

#else

    // enable all power except internal oscillator
    Registers.Power = FUSB302_D_Power_PWR_MASK; // & ~FUSB302_D_Power_PWR_InternalOscillator;
    if (write(FUSB302_D_Register_Power, Registers.Power) == false)
        return false;

    // enable high current mode
    // if we were using the interrupt pin, also set FUSB302_D_Control0_INT_MASK (don't forget to optionally set TOG_RD_ONLY)
    //	Registers.Control0 = (Registers.Control0 & ~FUSB302_D_Control0_HOST_CUR_MASK) | FUSB302_D_Control0_HOST_CUR_HighCurrentMode;

    //	Registers.Control0 &= ~FUSB302_D_Control0_AUTO_PRE; // is 0 by default
    //	if (write( FUSB302_D_Register_Control0, Registers.Control0 ) == false) return false;

    // ON SEMI also sets TOC_USRC_EXIT of "undocumented control 4"

    // enable sink polling (NOTE, we're not using it right now, disabled by default)
    //	read( FUSB302_D_Register_Control2 );
    //	Registers.Control2 = FUSB302_D_Control2_MODE_SnkPolling | FUSB302_D_Control2_TOGGLE;
    //	if (write( FUSB302_D_Register_Control2 );

    Registers.Switches1 = FUSB302_D_Switches1_POWERROLE_Sink | FUSB302_D_Switches1_DATAROLE_Sink | FUSB302_D_Switches1_SPECREV_Rev2_0 | FUSB302_D_Switches1_AUTO_CRC;
    if (write(FUSB302_D_Register_Switches1, Registers.Switches1) == false)
        return false;
    //	printf("configure Switches1 %02x\n", Registers.Switches1 );

    Registers.Control3 |= FUSB302_D_Control3_AUTO_HARDRESET | FUSB302_D_Control3_AUTO_SOFTRESET | FUSB302_D_Control3_AUTO_RETRY | (0xFF & FUSB302_D_Control3_N_RETRIES_MASK);
    if (write(FUSB302_D_Register_Control3, Registers.Control3) == false)
        return false;

    // disable all interrupts
    Registers.Mask1 = FUSB302_D_Mask1_ALL;
    if (write(FUSB302_D_Register_Mask1, Registers.Mask1) == false)
        return false;
    Registers.Maska = FUSB302_D_Maska_ALL;
    if (write(FUSB302_D_Register_Maska, Registers.Maska) == false)
        return false;
    Registers.Maskb = FUSB302_D_Maskb_ALL;
    if (write(FUSB302_D_Register_Maskb, Registers.Maskb) == false)
        return false;

//	printf("configure Switches1 %02x\n", Registers.Switches1 );
#endif

    return true;
}

/**
 * Tries to get fresh value of measurement (the currently selected cc pin)
 */
static uint8_t readCCVoltageLevel(uint8_t *bc_lvl)
{
    if (read(FUSB302_D_Register_Status0, &Registers.Status0) == false)
    {
        return false;
    }

    *bc_lvl = Registers.Status0 & FUSB302_D_Status0_BC_LVL_MASK;

    return true;
}

/**
 * Tries to detect which cc pin has voltage
 * WARNING: resets Switches0 register
 */
static CCHandshake_CC_t detectCCPinSink(void)
{
    uint8_t BC_LVL;

    //	read( FUSB302_D_Register_Switches0 );

    // enable measurement on cc1
    Registers.Switches0 = (Registers.Switches0 & ~FUSB302_D_Switches0_MEAS_CC_MASK) | FUSB302_D_Switches0_MEAS_CC1; // | FUSB302_D_Switches0_PDWN2 | FUSB302_D_Switches0_PDWN1;
    write(FUSB302_D_Register_Switches0, Registers.Switches0);

    // wait a bit
    DelayMs(250);

    // get measurement
    if (readCCVoltageLevel(&BC_LVL) == false)
    {
        return CCHandshake_CC_None;
    }
    if (BC_LVL > FUSB302_D_Status0_BC_LVL_LessThan200mV)
    {
        return CCHandshake_CC_1;
    }

    // enable measurement on cc2
    Registers.Switches0 = (Registers.Switches0 & ~FUSB302_D_Switches0_MEAS_CC_MASK) | FUSB302_D_Switches0_MEAS_CC2; // | FUSB302_D_Switches0_PDWN2 | FUSB302_D_Switches0_PDWN1;
    write(FUSB302_D_Register_Switches0, Registers.Switches0);

    // wait a bit
    DelayMs(250);

    // get measurement
    if (readCCVoltageLevel(&BC_LVL) == false)
    {
        return CCHandshake_CC_None;
    }
    if (BC_LVL > FUSB302_D_Status0_BC_LVL_LessThan200mV)
    {
        return CCHandshake_CC_2;
    }

    return CCHandshake_CC_None;
}

static uint8_t enableSink(CCHandshake_CC_t cc)
{
    if (cc == CCHandshake_CC_None)
    {
        printf("Trying to enable none!");
        return false;
    }

    //	printf("enableSink Switches1 %02x\n", Registers.Switches1 );
    //	read( FUSB302_D_Register_Switches1 );
    //	read( FUSB302_D_Register_Power );

    //	printf("switches1 %02x\n", Registers.Switches1 );

    // enable meas and txcc
    if (cc == CCHandshake_CC_1)
    {
        Registers.Switches0 = ((Registers.Switches0 & ~FUSB302_D_Switches0_MEAS_CC_MASK) | FUSB302_D_Switches0_MEAS_CC1);
        Registers.Switches1 = (Registers.Switches1 & ~FUSB302_D_Switches1_TXCC_MASK) | FUSB302_D_Switches1_TXCC1;
    }
    else if (cc == CCHandshake_CC_2)
    {
        Registers.Switches0 = ((Registers.Switches0 & ~FUSB302_D_Switches0_MEAS_CC_MASK) | FUSB302_D_Switches0_MEAS_CC2);
        Registers.Switches1 = (Registers.Switches1 & ~(FUSB302_D_Switches1_TXCC_MASK)) | FUSB302_D_Switches1_TXCC2;
    }

    if (write(FUSB302_D_Register_Switches0, Registers.Switches0) == false)
        return false;
    if (write(FUSB302_D_Register_Switches1, Registers.Switches1) == false)
        return false;

    //	read( FUSB302_D_Register_Switches1, &Registers.Switches1 );
    //	printf("enableSink Switches1 %02x\n", Registers.Switches1 );

    // enable oscillator for PD
    //	if ( (Registers.Power & FUSB302_D_Power_PWR_InternalOscillator) != FUSB302_D_Power_PWR_InternalOscillator )
    //	{
    //		printf("activating internal oscillator\n");
    //		Registers.Power |= FUSB302_D_Power_PWR_InternalOscillator;
    //		if (write( FUSB302_D_Register_Power, Registers.Power ) == false) return false;
    //	}

    //	Registers.Switches
    // TXCCx = 1
    // MEAS_CCx = 1
    // TXCCy = 0
    // MEAS_CCy = 0

    //	Registers.Switches1 // by default we don't have to set this
    // POWERROLE = 0
    // DATAROLE = 0

    // Registers.Control
    // ENSOP1 = 0
    // ENSOP1DP = 0
    // ENSOP2 = 0
    // ENSOP2DP = 0

    // Registers.Power enable internal oscillator

    return true;
}

static uint8_t disableSink()
{
    //	printf("disableSink()\n");

    //	read( FUSB302_D_Register_Switches1 );
    //	read( FUSB302_D_Register_Power );

    //	printf("disableSink Switches1 %02x\n", Registers.Switches1 );

    // disable TXCCx
    Registers.Switches1 &= ~FUSB302_D_Switches1_TXCC_MASK;
    write(FUSB302_D_Register_Switches1, Registers.Switches1);

    // disable CC
    Registers.Switches0 &= ~FUSB302_D_Switches0_MEAS_CC_MASK;
    write(FUSB302_D_Register_Switches0, Registers.Switches0);

    //	printf("disableSink Switches1 %02x\n", Registers.Switches1 );
    // disable oscillator for PD
    //	Registers.Power &= ~FUSB302_D_Power_PWR_InternalOscillator;

    return true;
}

static void pd_core(void)
{
    if (PD.State == PD_State_Disabled)
    {
        return;
    }

    // 检查超时
    if (PD.State == PD_State_Idle &&
        PD.Tx.SendAttempts > 0 &&
        (TimerGetCurrentTime() - PD.Tx.SentTs) > PD_REQUEST_TIMEOUT_MS)
    {
        printf("PD Request timeout, retrying...\n");
        if (PD.Tx.RetryCount < PD_MAX_RETRY_COUNT)
        {
            PD.Tx.RetryCount++;
            PD.Tx.SendAttempts = 0;
            pd_sendMessage(&PD.Tx.Message, NULL);
            PD.Tx.SentTs = TimerGetCurrentTime();
        }
        else
        {
            printf("PD Request failed after %d retries, resetting...\n", PD_MAX_RETRY_COUNT);
            PD.State = PD_State_Reset;
        }
        return;
    }

    // 处理各种中断标志
    if ((Registers.Interrupta & FUSB302_D_Interrupta_I_SOFTFAIL) == FUSB302_D_Interrupta_I_SOFTFAIL)
    {
        printf("I_SOFTFAIL\n");
    }
    if ((Registers.Interrupta & FUSB302_D_Interrupta_I_RETRYFAIL) == FUSB302_D_Interrupta_I_RETRYFAIL)
    {
        printf("I_RETRYFAIL\n");
    }
    if ((Registers.Interrupta & FUSB302_D_Interrupta_I_HARDSENT) == FUSB302_D_Interrupta_I_HARDSENT)
    {
        printf("I_HARDSENT\n");
    }
    if ((Registers.Interrupta & FUSB302_D_Interrupta_I_TXSENT) == FUSB302_D_Interrupta_I_TXSENT)
    {
        printf("I_TXSENT\n");
    }
    if ((Registers.Interrupta & FUSB302_D_Interrupta_I_SOFTRST) == FUSB302_D_Interrupta_I_SOFTRST)
    {
        printf("I_SOFTRST\n");
    }
    if ((Registers.Interrupta & FUSB302_D_Interrupta_I_HARDRST) == FUSB302_D_Interrupta_I_HARDRST)
    {
        printf("I_HARDRST\n");
        PD.State = PD_State_Reset;
    }

    if ((Registers.Interruptb & FUSB302_D_Interruptb_I_GCRCSENT) == FUSB302_D_Interruptb_I_GCRCSENT)
    {
        printf("I_GCRCSENT\n");
        PD.State = PD_State_Rx;
    }

    if ((Registers.Interrupt & FUSB302_D_Interrupt_I_COLLISION) == FUSB302_D_Interrupt_I_COLLISION)
    {
        printf("I_COLLISION\n");
    }
    if ((Registers.Interrupt & FUSB302_D_Interrupt_I_ACTIVITY) == FUSB302_D_Interrupt_I_ACTIVITY)
    {
        // printf("I_ACTIVITY\n");
    }
    if ((Registers.Interrupt & FUSB302_D_Interrupt_I_ALERT) == FUSB302_D_Interrupt_I_ALERT)
    {
        printf("I_ALERT\n");
    }

    // 检查RX状态
    if ((Registers.Status1 & FUSB302_D_Status1_RX_EMPTY) == FUSB302_D_Status1_RX_EMPTY)
    {
        // printf("RX_EMPTY\n");
        PD.Rx.HasData |= PD.Rx.HasData;
        PD.State = PD_State_Rx;
    }
    if ((Registers.Status1 & FUSB302_D_Status1_RX_FULL) == FUSB302_D_Status1_RX_FULL)
    {
        // printf("RX_FULL\n");
    }
    if ((Registers.Status1 & FUSB302_D_Status1_TX_EMPTY) == FUSB302_D_Status1_TX_EMPTY)
    {
        // printf("TX_EMPTY\n");
    }
    if ((Registers.Status1 & FUSB302_D_Status1_TX_FULL) == FUSB302_D_Status1_TX_FULL)
    {
        // printf("TX_FULL\n");
    }

    // 处理PD状态机
    switch (PD.State)
    {
    case PD_State_Disabled:
    {
        break;
    }

    case PD_State_HardReset:
    {
        printf("PD State HardReset\n");
        pd_hardreset();
        DelayMs(1);
        PD.State = PD_State_Reset;
        break;
    }

    case PD_State_Reset:
    {
        printf("PD State Reset\n");
        pd_reset();
        pd_flushRxFifo();
        pd_flushTxFifo();

        // 尝试获取源能力
        pd_clearTx();
        PD_newMessage(&PD.Tx.Message, 0, pd_nextTxMessageId(),
                      PD_HeaderWord_PowerRole_Sink, PD_HeaderWord_SpecRev_2_0,
                      PD_HeaderWord_DataRole_Sink, PD_ControlCommand_GetSourceCap, NULL);
        pd_sendMessage(&PD.Tx.Message, NULL);
        PD.Tx.SendAttempts = 0;
        break;
    }

    case PD_State_Idle:
    {
        // 检查是否有待发送的消息
        if ((Registers.Status1 & FUSB302_D_Status1_TX_EMPTY) != FUSB302_D_Status1_TX_EMPTY &&
            (Registers.Status1 & FUSB302_D_Status1_TX_FULL) != FUSB302_D_Status1_TX_FULL)
        {
            // 有消息待发送，但这里不需要特殊处理
        }
        break;
    }

    case PD_State_Rx:
    {
        if (pd_hasMessage() == false)
        {
            PD.State = PD_State_Idle;
            return;
        }
        printf("PD has message\n");

        memset(&PD.Rx.Message, 0, sizeof(PD_Message_t));

        if (pd_getMessage(&PD.Rx.Message) == false)
        {
            PD.State = PD_State_Idle;
            return;
        }

        PD.State = pd_processMessage(&PD.Rx.Message);
        break;
    }

    default:
        PD.State = PD_State_Reset;
    }
}

static void pd_init(void)
{
    //	printf("pd_init Switches1 %02x\n", Registers.Switches1 );

    PD.Rx.HasData = false;

    PD.Tx.MessageId = 2;
    PD.State = PD_State_Idle;

    PD.Power.NSourceCapabilities = 0;
    memset(&PD.Power.SourceCapabilities[0], 0, PD_MESSAGE_MAX_OBJECTS * sizeof(PD_DataObject_t));
    PD.Power.BestCapIndex = 0;

    // === 构造 Get_Source_Cap 控制消息 ===
    PD_newMessage(&PD.Tx.Message, 0, pd_nextTxMessageId(), PD_HeaderWord_PowerRole_Sink, PD_HeaderWord_SpecRev_2_0, PD_HeaderWord_DataRole_Sink, PD_ControlCommand_GetSourceCap, NULL);

    pd_sendMessage(&PD.Tx.Message, NULL);
    // // === 清空 FIFO ===
    // pd_flushRxFifo();
    // pd_flushTxFifo();

    // // === 构造 Get_Source_Cap 控制消息 ===
    // PD_newMessage(
    //     &PD.Tx.Message,
    //     0,                              // 无 Data Object
    //     pd_nextTxMessageId(),           // 自动消息号
    //     PD_HeaderWord_PowerRole_Sink,   // 我是 Sink
    //     PD_HeaderWord_SpecRev_2_0,      // 支持 PD 2.0
    //     PD_HeaderWord_DataRole_Sink,    // 数据角色 Sink
    //     PD_ControlCommand_GetSourceCap, // 控制命令：请求源能力
    //     NULL);

    // // === 发送消息 ===
    // pd_sendMessage(&PD.Tx.Message, NULL);
    // DelayMs(100);

    // // === 等待回复 ===
    // uint8_t try = 10;
    // while (try--)
    // {
    //     uint8_t status1 = 0;
    //     fusb_readRegister(FUSB302_D_Register_Status1, &status1);

    //     if (status1 & FUSB302_D_Status1_RX_FULL)
    //     {
    //         // 收到消息，尝试解析
    //         if (pd_getMessage(&PD.Rx.Message) == false)
    //         {
    //             printf("Failed to get PD message\n");
    //             return;
    //         }

    //         uint8_t nobj = PD_HeaderWord_getNumberOfDataObjects(PD.Rx.Message.Header.Word);
    //         PD.Power.NSourceCapabilities = nobj;

    //         printf("Received Source Capabilities (%d PDOs):\n", nobj);

    //         // 从消息体中解析 Data Object，每个 4 字节
    //         for (uint8_t i = 0; i < nobj; i++)
    //         {
    //             PD_DataObject_t pdo;
    //             uint8_t *p = ((uint8_t *)&PD.Rx.Message) +2 + (i * 4);

    //             // 安全取出 4 字节（根据你库中数据存放的位置微调）
    //             memcpy(&pdo.Value, p, 4);

    //             PD.Power.SourceCapabilities[i] = pdo;

    //             uint32_t raw = pdo.Value;
    //             uint8_t type = (raw >> 30) & 0x03;
    //             uint16_t mv = ((raw >> 10) & 0x3FF) * 50; // 电压 50mV 步进
    //             uint16_t ma = (raw & 0x3FF) * 10;         // 电流 10mA 步进

    //             printf("  PDO[%d]: ", i);
    //             switch (type)
    //             {
    //             case 0:
    //                 printf("Fixed Supply ");
    //                 break;
    //             case 1:
    //                 printf("Battery ");
    //                 break;
    //             case 2:
    //                 printf("Variable Supply ");
    //                 break;
    //             case 3:
    //                 printf("Augmented (PPS) ");
    //                 break;
    //             default:
    //                 printf("Unknown ");
    //                 break;
    //             }

    //             printf(" - %u mV, %u mA, RAW=0x%08X\n", mv, ma, raw);
    //         }

    //         printf("  PD Spec Revision: PD2.0\n");
    //         return;
    //     }

    //     DelayMs(50);
    // }

    // printf("No Source Capabilities received.\n");

    // enable auto goodCRC
    //	printf("Switches1 %d\n", Registers.Switches1 );
    //	read( FUSB302_D_Register_Switches1 );
    //	printf("Switches1 %d\n", Registers.Switches1 );
    //	Registers.Switches1 = FUSB302_D_Switches1_SPECREV_Rev2_0 | FUSB302_D_Switches1_AUTO_CRC;
    //	Registers.Switches1 = (Registers.Switches1 & ~FUSB302_D_Switches1_SPECREV_MASK) | FUSB302_D_Switches1_SPECREV_Rev2_0;
    //	Registers.Switches1 |= FUSB302_D_Switches1_AUTO_CRC;

    // enable meas and txcc
    //	if (ConnectedCC == CCHandshake_CC_1)
    //	{
    ////		Registers.Switches0 = (Registers.Switches0 & ~)
    //		Registers.Switches1 |= FUSB302_D_Switches1_TXCC1;
    //	}
    //	else if (ConnectedCC == CCHandshake_CC_2)
    //	{
    //		Registers.Switches1 |= FUSB302_D_Switches1_TXCC2;
    //
    //	}

    //	printf("Switches1 %d\n", Registers.Switches1 );
    //	write( FUSB302_D_Register_Switches1 );
    //
    //	printf("Switches1 %d\n", Registers.Switches1 );
    //
    //	read( FUSB302_D_Register_Switches1 );
    //	printf("Switches1 %d\n", Registers.Switches1 );

    //	read( FUSB302_D_Register_Control0, Registers.Control0 );
    //	Registers.Control0 &= ~FUSB302_D_Control0_AUTO_PRE;
    //	write( FUSB302_D_Register_Control0, Registers.Control0 );

    // enable interrupts
    //	read( FUSB302_D_Register_Maska );
    //	Registers.Maska |= FUSB302_D_Maska_M_TXSENT;
    //	write( FUSB302_D_Register_Maska, Registers.Maska );

    //	read( FUSB302_D_Register_Maskb );
    //	Registers.Maskb |= FUSB302_D_Maskb_M_GCRCSENT;
    //	write( FUSB302_D_Register_Maskb, Registers.Maskb );

    //	pd_reset();
    //	pd_flushRxFifo();
    //	pd_flushTxFifo();

    //	printf("pd_init Switches1 %02x\n", Registers.Switches1 );
}

static void pd_deinit(void)
{
    //	printf("pd_deinit Switches1 %02x\n", Registers.Switches1 );

    PD.State = PD_State_Disabled;

    PD.Tx.MessageId = 2;

    PD.Power.NSourceCapabilities = 0;

    pd_reset();
    pd_flushRxFifo();
    pd_flushTxFifo();

    // disable auto goodCRC
    //	read( FUSB302_D_Register_Switches1 );
    //	Registers.Switches1 |= FUSB302_D_Switches1_AUTO_CRC;
    //	write( FUSB302_D_Register_Switches1, Registers.Switches1 );

    // disable interrupts
    //	read( FUSB302_D_Register_Maska );
    Registers.Maska &= ~FUSB302_D_Maska_M_TXSENT;
    write(FUSB302_D_Register_Maska, Registers.Maska);

    //	read( FUSB302_D_Register_Maskb );
    Registers.Maskb &= ~FUSB302_D_Maskb_M_GCRCSENT;
    write(FUSB302_D_Register_Maskb, Registers.Maskb);

    //	printf("pd_deinit Switches1 %02x\n", Registers.Switches1 );
}

static void pd_hardreset(void)
{
    PD.Tx.MessageId = 2;
    PD.Tx.RetryCount = 0;
    PD.Tx.SendAttempts = 0;

    // 发送HardReset
    Registers.Control3 |= FUSB302_D_Control3_SEND_HARD_RESET;
    write(FUSB302_D_Register_Control3, Registers.Control3);
    Registers.Control3 &= ~FUSB302_D_Control3_SEND_HARD_RESET;
}

static void pd_reset(void)
{
    //	PD.Tx.MessageId = 0;

    //	read(FUSB302_D_Register_Reset);

    Registers.Reset |= FUSB302_D_Reset_PD_RESET;
    write(FUSB302_D_Register_Reset, Registers.Reset);
    Registers.Reset &= ~FUSB302_D_Reset_PD_RESET;
}

static void pd_flushRxFifo(void)
{
    //	read( FUSB302_D_Register_Control1 );

    Registers.Control1 |= FUSB302_D_Control1_RX_FLUSH;

    write(FUSB302_D_Register_Control1, Registers.Control1);

    // clear bit again
    Registers.Control1 &= ~FUSB302_D_Control1_RX_FLUSH;
}

static void pd_flushTxFifo(void)
{
    //	read( FUSB302_D_Register_Control0 );

    Registers.Control0 |= FUSB302_D_Control0_TX_FLUSH;

    write(FUSB302_D_Register_Control0, Registers.Control0);

    // clear bit again
    Registers.Control0 &= ~FUSB302_D_Control0_TX_FLUSH;
}

static uint8_t pd_hasMessage(void)
{
    //	read( FUSB302_D_Register_Status1, &Registers.Status1 );

    return (Registers.Status1 & FUSB302_D_Status1_RX_EMPTY) != FUSB302_D_Status1_RX_EMPTY;
}

static void pd_startTx(void)
{
    //	read( FUSB302_D_Register_Control0 );
    Registers.Control0 |= FUSB302_D_Control0_TX_START;
    write(FUSB302_D_Register_Control0, Registers.Control0);
    Registers.Control0 &= ~FUSB302_D_Control0_TX_START;
}
// static void pd_setRoles( CCHandshake_PD_Role_t powerRole, CCHandshake_PD_Role_t dataRole )
//{
//	read( FUSB302_D_Register_Switches1 );
//
//	// clear bits
//	Registers.Switches1 &= ~(FUSB302_D_Switches1_POWERROLE | FUSB302_D_Switches1_DATAROLE);
//
//	if (powerRole == CCHandshake_PD_Role_Source)
//	{
//		Registers.Switches1 |= FUSB302_D_Switches1_POWERROLE_Source;
//	}
//	else
//	{
//		Registers.Switches1 |= FUSB302_D_Switches1_POWERROLE_Sink;
//	}
//
//	if (dataRole == CCHandshake_PD_Role_Source)
//	{
//		Registers.Switches1 |= FUSB302_D_Switches1_DATAROLE_Source;
//	}
//	else
//	{
//		Registers.Switches1 |= FUSB302_D_Switches1_DATAROLE_Sink;
//	}
//
//	write( FUSB302_D_Register_Switches1 );
// }
//
// static void pd_setAutoGoodCrc( uint8_t enabled )
//{
//	read( FUSB302_D_Register_Switches1 );
//
//	Registers.Switches1 &= ~FUSB302_D_Switches1_AUTO_CRC;
//
//	if (enabled)
//	{
//		Registers.Switches1 |= FUSB302_D_Switches1_AUTO_CRC;
//	}
//
//	write( FUSB302_D_Register_Switches1 );
// }

static uint8_t pd_getMessage(PD_Message_t *message)
{
    uint8_t token;

    do
    {

        token = 0;

        if (FUSB302_D_Read(&Driver, FUSB302_D_Register_FIFOs, &token) == FUSB302_D_ERROR)
        {
            printf("failed read 1\n");
            return false;
        }

        //		printf(" %02x \n", addr);

        // discard and abort if not right type
        if ((token & FUSB302_D_RxFIFOToken_MASK) != FUSB302_D_RxFIFOToken_SOP)
        {
            printf("%02x", token);
            //			return false;
            read(FUSB302_D_Register_Status1, &Registers.Status1);
            if ((Registers.Status1 & FUSB302_D_Status1_RX_EMPTY) == FUSB302_D_Status1_RX_EMPTY)
            {
                printf("\nnothing left\n");
                return false;
            }
        }
        else
        {
            //			printf("- %02x\n", token);
            break;
        }

        //		read( FUSB302_D_Register_Status1 );

    } while (1);

    // // 打印 header
    // uint8_t cmd = PD_HeaderWord_getCommandCode(message->Header.Word);
    // uint8_t nobj = PD_HeaderWord_getNumberOfDataObjects(message->Header.Word);
    // printf("PD msg: cmd=%d, nobj=%d, hdr=0x%04X\n", cmd, nobj, message->Header.Word);

    // // 打印所有 data object（如果有）
    // for (uint8_t i = 0; i < nobj; i++)
    // {
    //     printf("  PDO[%d] = 0x%08lX\n", i, (unsigned long)message->DataObjects[i].Value);
    // }

    uint8_t header[2] = {0, 0};

    if (FUSB302_D_ReadN(&Driver, FUSB302_D_Register_FIFOs, &header[0], 2) == FUSB302_D_ERROR)
    {
        printf("failed read 2\n");
        return false;
    }

    le_to_u16(&message->Header.Word, header);

    PD.Rx.MessageId = PD_HeaderWord_getMessageId(message->Header.Word);
    //	PD.Tx.MessageId = PD.Rx.MessageId + 1;

    //	printf("RX tkn %02x hdr %02x%02x\n", token, message->Header.Bytes[0], message->Header.Bytes[1] );

    uint8_t N = PD_HeaderWord_getNumberOfDataObjects(message->Header.Word);

    if (N > 0)
    {
        uint8_t buf[PD_MESSAGE_MAX_OBJECTS * sizeof(PD_DataObject_t)];
        memset(buf, 0, PD_MESSAGE_MAX_OBJECTS * sizeof(PD_DataObject_t));

        if (FUSB302_D_ReadN(&Driver, FUSB302_D_Register_FIFOs, &buf[0], sizeof(PD_DataObject_t) * N) == FUSB302_D_ERROR)
        //		if (FUSB302_D_ReadN( &Driver, FUSB302_D_Register_FIFOs, &message->DataObjects[0], sizeof(PD_DataObject_t) * N ) == FUSB302_D_ERROR)
        {
            printf("failed read 3\n");
            return false;
        }

        for (uint8_t n = 0; n < N; n++)
        {
            //			message->DataObjects[n] = buf[n*sizeof(PD_DataObject_t)];
            le_to_u32(&message->DataObjects[n], &buf[n * sizeof(PD_DataObject_t)]);

            //			printf("%08x vs %02x%02x%02x%02x\n",
            //					message->DataObjects[n],
            //					buf[n*sizeof(PD_DataObject_t)],
            //					buf[n*sizeof(PD_DataObject_t)+1],
            //					buf[n*sizeof(PD_DataObject_t)+2],
            //					buf[n*sizeof(PD_DataObject_t)+3]);
        }
    }

    uint8_t crc32[4] = {0, 0, 0, 0};

    if (FUSB302_D_ReadN(&Driver, FUSB302_D_Register_FIFOs, &crc32[0], 4) == FUSB302_D_ERROR)
    {
        printf("failed read 4\n");
        return false;
    }

    le_to_u32(&message->Crc32, crc32);

    return true;
}

static uint8_t pd_sendMessage(PD_Message_t *message, PD_State_t (*onAcknowledged)(PD_Message_t *message))
{
    uint8_t N = PD_HeaderWord_getNumberOfDataObjects(message->Header.Word);

    printf("---- PD tx\n");
    printf("mid = %d prole = %d spec = %d drole = %d cmd = %d\n",
        PD_HeaderWord_getMessageId(message->Header.Word),
        PD_HeaderWord_getPowerRole(message->Header.Word),
        PD_HeaderWord_getSpecRev(message->Header.Word),
        PD_HeaderWord_getDataRole(message->Header.Word),
        PD_HeaderWord_getCommandCode(message->Header.Word));

    printf("data (%d) ", N);
    for (uint8_t n = 0; n < N; n++)
    {
        printf("%08x", message->DataObjects[n].Value);
    }
    printf("\n----\n");

    PD.Tx.OnAcknowledged = onAcknowledged;
    PD.Tx.SendAttempts++;
    PD.Tx.SentTs = TimerGetCurrentTime();

    // 发送消息的代码保持不变...
    uint8_t buf[64];
    memset((uint8_t *)&buf[0], 0, sizeof(buf));

    uint8_t i = 0;
    buf[i++] = FUSB302_D_TxFIFOToken_SOP1;
    buf[i++] = FUSB302_D_TxFIFOToken_SOP1;
    buf[i++] = FUSB302_D_TxFIFOToken_SOP1;
    buf[i++] = FUSB302_D_TxFIFOToken_SOP2;
    buf[i++] = FUSB302_D_TxFIFOToken_PACKSYM | (2 + N * sizeof(PD_DataObject_t));

    u16_to_le(&buf[i], message->Header.Word);
    i += 2;

    for (uint8_t n = 0; n < N; n++)
    {
        u32_to_le(&buf[i], message->DataObjects[n].Value);
        i += sizeof(PD_DataObject_t);
    }

    buf[i++] = FUSB302_D_TxFIFOToken_JAM_CRC;
    buf[i++] = FUSB302_D_TxFIFOToken_EOP;
    buf[i++] = FUSB302_D_TxFIFOToken_TXOFF;
    buf[i++] = FUSB302_D_TxFIFOToken_TXON;

    if (FUSB302_D_WriteN(&Driver, FUSB302_D_Register_FIFOs, &buf[0], i) == FUSB302_D_ERROR)
    {
        printf("Write FAIL\n");
        return false;
    }

    printf("Tx (%d)\n", i);
    PD.State = PD_State_Idle;

    return true;
}
static PD_State_t pd_processMessage(PD_Message_t *message)
{
    uint8_t N = PD_HeaderWord_getNumberOfDataObjects(message->Header.Word);

    printf("---- PD rx\n");
    printf("mid = %d prole = %d spec = %d drole = %d cmd = %d\n",
        PD_HeaderWord_getMessageId(message->Header.Word),
        PD_HeaderWord_getPowerRole(message->Header.Word),
        PD_HeaderWord_getSpecRev(message->Header.Word),
        PD_HeaderWord_getDataRole(message->Header.Word),
        PD_HeaderWord_getCommandCode(message->Header.Word));

    printf("data (%d) ", N);
    for (uint8_t n = 0; n < N; n++)
    {
        printf("%08x", message->DataObjects[n].Value);
    }
    printf("\n----\n");

    if (PD_isControlMessage(message))
    {
        switch (PD_HeaderWord_getCommandCode(message->Header.Word))
        {
        case PD_ControlCommand_GoodCRC:
        {
            printf("GoodCRC received!\n");
            if (PD.Tx.OnAcknowledged != NULL)
            {
                return PD.Tx.OnAcknowledged(message);
            }
            // 重置重试计数
            PD.Tx.RetryCount = 0;
            PD.Tx.SendAttempts = 0;
            break;
        }

        case PD_ControlCommand_Accept:
        {
            printf("Accept! PD negotiation successful\n");
            // 重置重试计数
            PD.Tx.RetryCount = 0;
            PD.Tx.SendAttempts = 0;
            // 协商成功，可以进入正常工作状态
            break;
        }

        case PD_ControlCommand_Reject:
        {
            printf("Reject! PD negotiation failed\n");
            // 重置重试计数
            PD.Tx.RetryCount = 0;
            PD.Tx.SendAttempts = 0;
            // 协商失败，可能需要重新协商
            break;
        }

        case PD_ControlCommand_SoftReset:
        {
            printf("SoftReset received\n");
            return PD_State_Reset;
        }

        default:
            printf("Ignoring control %d\n", PD_HeaderWord_getCommandCode(message->Header.Word));
        }
    }
    else
    {
        switch (PD_HeaderWord_getCommandCode(message->Header.Word))
        {
        case PD_DataCommand_SourceCapabilities:
        {
            return pd_onSourceCapabilities(message);
        }
        default:
            printf("Ignoring data %d\n", PD_HeaderWord_getCommandCode(message->Header.Word));
        }
    }

    return PD_State_Idle;
}
static PD_State_t pd_onSourceCapabilities(PD_Message_t *message)
{
    uint8_t N = PD_HeaderWord_getNumberOfDataObjects(message->Header.Word);
    
    if (N == 0)
    {
        printf("no data\n");
        return PD_State_Reset;
    }
    
    PD.Power.NSourceCapabilities = N;
    memcpy(&PD.Power.SourceCapabilities[0], &message->DataObjects[0], N * sizeof(PD_DataObject_t));
    
    if (N == 1)
    {
        return PD_State_Idle;
    }
    
    // 强制选择第一个PDO（通常是5V）
    PD.Power.BestCapIndex = 1; // 选择第一个PDO
    
    printf("Available Source Capabilities (%d PDOs):\n", N);
    
    for (uint8_t i = 0; i < N; i++)
    {
        uint32_t caps = PD.Power.SourceCapabilities[i].Value;
        switch (caps & PDO_SrcCap_SupplyType_MASK)
        {
        case PDO_SrcCap_SupplyType_Fixed:
        {
            uint32_t v = PDO_SrcCap_Fixed_getVoltage_50mV(caps);
            uint32_t c = PDO_SrcCap_Fixed_getMaxCurrent_10mA(caps);
            printf("%d fixed  %d mV max %d mA\n", i, 50 * v, 10 * c);
            break;
        }
        }
    }
    
    printf("BestCapIndex = %d (forced to first PDO)\n", PD.Power.BestCapIndex);
    
    // 创建请求
    pd_createRequest(&PD.Tx.Message);
    
    // 重置重试计数
    PD.Tx.RetryCount = 0;
    PD.Tx.SendAttempts = 0;
    
    // 发送请求
    pd_sendMessage(&PD.Tx.Message, NULL);
    
    return PD_State_Idle;
}
static void pd_createRequest(PD_Message_t *message)
{
    if (PD.Power.BestCapIndex == 0)
    {
        return;
    }
    
    DelayMs(1);
    
    uint32_t caps;
    uint32_t v, c;
    
    // 确保索引正确
    caps = PD.Power.SourceCapabilities[PD.Power.BestCapIndex - 1].Value;
    
    switch (caps & PDO_SrcCap_SupplyType_MASK)
    {
    case PDO_SrcCap_SupplyType_Fixed:
    {
        v = PDO_SrcCap_Fixed_getVoltage_50mV(caps);
        c = PDO_SrcCap_Fixed_getMaxCurrent_10mA(caps);
        
        // 使用非常保守的电流请求（30%的最大电流）
        uint32_t conservative_current = (c * 3) / 10; // 30% of max current
        
        // 确保不超过我们的最大限制
        if (10 * conservative_current > PD_REQUEST_MAX_MILLIAMP)
        {
            conservative_current = PD_REQUEST_MAX_MILLIAMP / 10;
        }
        
        // 确保最小电流
        if (conservative_current < 10) // 至少100mA
        {
            conservative_current = 10;
        }
        
        // 创建Request数据对象
        PD_DataObject_t request = {.Value = 0};
        request.Value = PDO_Req_Fixed_NoUSBSuspend;
        request.Value |= PDO_Req_Fixed_setObjectPosBits(PD.Power.BestCapIndex);
        request.Value |= PDO_Req_Fixed_setOperatingCurrent_10mABits(conservative_current);
        request.Value |= PDO_Req_Fixed_setMaxOpCur_10mABits(conservative_current);
        
        printf("Requesting PDO[%d]: %lu mV @ %lu mA (very conservative)\n",
               PD.Power.BestCapIndex, v * 50, conservative_current * 10);
        
        PD_newMessage(message, 1, pd_nextTxMessageId(),
                      PD_HeaderWord_PowerRole_Sink, PD_HeaderWord_SpecRev_2_0,
                      PD_HeaderWord_DataRole_Sink, PD_DataCommand_Request, &request);
        break;
    }
    }
}
// static void pd_createRequest(PD_Message_t *message)
// {
//     if (PD.Power.NSourceCapabilities == 0)
//     {
//         printf("No source capabilities available\n");
//         return;
//     }

//     uint32_t caps;
//     uint32_t v, c;
//     uint32_t maxPower = 0;
//     uint8_t maxIndex = 0;
//     PD_DataObject_t request = {.Value = 0};

//     // 遍历所有源能力，选择固定电压类型中最大功率档位
//     for (uint8_t i = 0; i < PD.Power.NSourceCapabilities; i++)
//     {
//         caps = PD.Power.SourceCapabilities[i].Value;

//         if ((caps & PDO_SrcCap_SupplyType_MASK) == PDO_SrcCap_SupplyType_Fixed)
//         {
//             v = PDO_SrcCap_Fixed_getVoltage_50mV(caps);    // 50mV 单位
//             c = PDO_SrcCap_Fixed_getMaxCurrent_10mA(caps); // 10mA 单位

//             uint32_t power = v * 50 * c * 10; // mV*mA = μW, 简单比较功率大小

//             if (power > maxPower)
//             {
//                 maxPower = power;
//                 maxIndex = i;
//             }
//         }
//     }

//     // 使用最大功率档位
//     caps = PD.Power.SourceCapabilities[maxIndex].Value;
//     v = PDO_SrcCap_Fixed_getVoltage_50mV(caps);
//     c = PDO_SrcCap_Fixed_getMaxCurrent_10mA(caps);

//     // 限制请求电流
//     uint32_t current_mA = c * 10;
//     if (current_mA > PD_REQUEST_MAX_MILLIAMP)
//     {
//         current_mA = PD_REQUEST_MAX_MILLIAMP;
//         c = current_mA / 10;
//     }

//     // 构造 Request 数据对象
//     request.Value = PDO_Req_Fixed_NoUSBSuspend;
//     request.Value |= PDO_Req_Fixed_setObjectPosBits(maxIndex + 1); // PDO 索引从1开始
//     request.Value |= PDO_Req_Fixed_setOperatingCurrent_10mABits(c);
//     request.Value |= PDO_Req_Fixed_setMaxOpCur_10mABits(c);

//     printf("Requesting PDO[%d]: %lu mV @ %lu mA\n", maxIndex, v * 50, current_mA);

//     // 创建并发送 Request 消息，使用 PD 3.0 (spec=2)
//     PD_newMessage(message,
//                   1, // data object count
//                   pd_nextTxMessageId(),
//                   PD_HeaderWord_PowerRole_Sink,
//                   PD_HeaderWord_SpecRev_2_0,
//                   PD_HeaderWord_DataRole_Sink,
//                   PD_DataCommand_Request,
//                   &request);
// }

TimerTime_t TimerGetCurrentTime(void)
{
    return SYS_GetSysTickCnt(); // HAL 自带的毫秒计时器
}
    
#endif
