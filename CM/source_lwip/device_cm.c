//*****************************************************************************
//
// ethernetHandler.c - Driverlib functionalities to support FreeRTOS porting.
//
// Copyright (c) 2017 Texas Instruments Incorporated.  All rights reserved.
// Software License Agreement
//
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions
//   are met:
//
//   Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
//   Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the
//   distribution.
//
//   Neither the name of Texas Instruments Incorporated nor the names of
//   its contributors may be used to endorse or promote products derived
//   from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//*****************************************************************************

#include "device_cm.h"


/******************************************************************************
*These are defined by the linker (see device linker command file)
******************************************************************************/
extern uint16_t RamfuncsLoadStart;
extern uint16_t RamfuncsLoadSize;
extern uint16_t RamfuncsRunStart;
extern uint16_t RamfuncsLoadEnd;
extern uint16_t RamfuncsRunEnd;
extern uint16_t RamfuncsRunSize;

extern uint16_t constLoadStart;
extern uint16_t constLoadEnd;
extern uint16_t constLoadSize;
extern uint16_t constRunStart;
extern uint16_t constRunEnd;
extern uint16_t constRunSize;

#define DEVICE_FLASH_WAITSTATES 2

//-----------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------

void Custom_CM_init(void)
{
    //
    // Disable the watchdog
    //
    SysCtl_disableWatchdog();

    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_TIMER0);

#ifdef _FLASH
    //
    // Copy time critical code and flash setup code to RAM. This includes the
    // following functions: InitFlash();
    //
    // The RamfuncsLoadStart, RamfuncsLoadSize, and RamfuncsRunStart symbols
    // are created by the linker. Refer to the device .cmd file.
    // Html pages are also being copied from flash to ram.
    //
    memcpy(&RamfuncsRunStart, &RamfuncsLoadStart, (size_t)&RamfuncsLoadSize);
    memcpy(&constRunStart, &constLoadStart, (size_t)&constLoadSize);
    //
    // Call Flash Initialization to setup flash waitstates. This function must
    // reside in RAM.
    //
    Flash_initModule(FLASH0CTRL_BASE, FLASH0ECC_BASE, DEVICE_FLASH_WAITSTATES);
#endif

    //
    // Sets the NVIC vector table offset address.
    //
#ifdef _FLASH
    Interrupt_setVectorTableOffset((uint32_t)vectorTableFlash);
#else
    Interrupt_setVectorTableOffset((uint32_t)vectorTableRAM);
#endif

}

//------------------------------------------------Timer0---------------------------------------------------------------------------



/***********************************************************************************************
 * initTimer0
 * 
 * This function initializes CPU Timer 0.
 * It sets up the timer period, pre-scaler, and registers the ISR. 
 * 
 ***********************************************************************************************/
void 
initTimer0(void(*timer0IntHandler)(void))
{
    Interrupt_registerHandler(INT_TIMER0, timer0IntHandler);
    CPUTimer_setPeriod(CPUTIMER0_BASE, 0xFFFFFFFF);
    CPUTimer_setPreScaler(CPUTIMER0_BASE, 0);
    CPUTimer_stopTimer(CPUTIMER0_BASE);
    CPUTimer_reloadTimerCounter(CPUTIMER0_BASE);
}

/***********************************************************************************************
 * configCPUTimer
 * 
 * This function configures CPU Timers with a specified period in us.
 * 
***********************************************************************************************/
void
configCPUTimer0(uint32_t period)
{
    uint32_t temp, freq = CM_CLK_FREQ;

    //
    // Initialize timer period:
    //
    temp = ((freq / 1000000) * period);
    CPUTimer_setPeriod(CPUTIMER0_BASE, temp - 1);

    //
    // Set pre-scale counter to divide by 1 (SYSCLKOUT):
    //
    CPUTimer_setPreScaler(CPUTIMER0_BASE, 0);

    //
    // Initializes timer control register. The timer is stopped, reloaded,
    // free run disabled, and interrupt enabled.
    // Additionally, the free and soft bits are set
    //
    CPUTimer_stopTimer(CPUTIMER0_BASE);
    CPUTimer_reloadTimerCounter(CPUTIMER0_BASE);
    CPUTimer_setEmulationMode(CPUTIMER0_BASE,
                              CPUTIMER_EMULATIONMODE_STOPAFTERNEXTDECREMENT);
    CPUTimer_enableInterrupt(CPUTIMER0_BASE);

}
/*************************************************************************************************** 
* startTimer0
*
* Initiates the timer.
*
****************************************************************************************************/
void startTimer0(void)
{
    CPUTimer_startTimer(CPUTIMER0_BASE);
}

    




