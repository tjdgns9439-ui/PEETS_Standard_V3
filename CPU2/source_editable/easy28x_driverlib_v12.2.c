/***************************************************************
    easy28x_driverlib.c
	Copyright 2020. easyDSP All rights reserved
    DON'T CHANGE THIS FILE
    소스 파일이 아닌 헤더 파일을 변경하셔야 합니다.
****************************************************************/
#include "easy28x_driverlib_v12.2.h"
#include "device_not_editable/driverlib.h"
#include "device_not_editable/device.h"

// v10.1 : to support legacy definitions of pin mux info for gpio.c
#ifndef GPIO_28_SCIRXDA
#define GPIO_28_SCIRXDA     GPIO_28_SCIA_RX
#define GPIO_29_SCITXDA     GPIO_29_SCIA_TX
#define GPIO_85_SCIRXDA     GPIO_85_SCIA_RX
#define GPIO_84_SCITXDA     GPIO_84_SCIA_TX
#define GPIO_87_SCIRXDB     GPIO_87_SCIB_RX
#define GPIO_86_SCITXDB     GPIO_86_SCIB_TX
#define GPIO_15_SCIRXDB     GPIO_15_SCIB_RX
#define GPIO_14_SCITXDB     GPIO_14_SCIB_TX
#define GPIO_85_CMUARTRXA   GPIO_85_UARTA_RX
#define GPIO_84_CMUARTTXA   GPIO_84_UARTA_TX
#endif

// function declaration
__interrupt void easy_RXINT_ISR(void);
void easyDSP_SCIBootCPU2(void);
void easyDSP_SCIBootCM(void);                // for F2838xS and F2838xD
void easyDSP_FlashBootCPU2(void);
void easyDSP_2838x_Sci_Boot_Sync(void);
void easyDSP_28P65x_Sci_Boot_Sync(void);

#define F2838xX_CPU1    (F2838xS_CPU1       || F2838xD_CPU1)
#define F2838xX_CPU1_CM (F2838xS_CPU1_CM    || F2838xD_CPU1_CM)
#define F2838xX_ALL     (F2838xS_CPU1       || F2838xS_CPU1_CM || F2838xD_CPU1 || F2838xD_CPU1_CPU2 || F2838xD_CPU1_CM || F2838xD_CPU1_CPU2_CM)

////////////////////////////////////////////////////////////
// SCI-A used except that SCI-B is used for CPU2
// caution : to use SCIC or SCID for CPU2, you have to change the source yourself
//////////////////////////////////////////////////////////////
//#if (F2837xD_CPU1_CPU2 || F28P65xD_CPU1_CPU2) && defined(CPU2)
//#define SCIx_BASE   SCIB_BASE
//#elif (F2838xD_CPU1_CPU2 || F2838xD_CPU1_CPU2_CM) && defined(CPU2)
#if defined(CPU2)
uint32_t SCIx_BASE = SCIB_BASE;
#else
uint32_t SCIx_BASE = SCIA_BASE;
#endif

// for internal use. Don't delete !!!!28
// v10.1 : moved to here
#define VER_OF_THIS_FILE    1220    // version xx.y.z
volatile unsigned int ezDSP_Version_SCI = VER_OF_THIS_FILE;                     // don't change the variable name
volatile unsigned int ezDSP_uRead16BPossible = 1, ezDSP_uRead8BPossible = 1;    // don't change the variable name
bool ezDSP_b32bitAddress = false;                                               // don't change the variable name

#if defined(CPU1)
unsigned int ezDSP_uCPU = 1;
#elif defined(CPU2)
unsigned int ezDSP_uCPU = 2;
#else
#define CPU1
unsigned int ezDSP_uCPU = 1;
#endif

unsigned int ezDSP_uOnChipFlash = 0;

/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// SCI initialization
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void easyDSP_SCI_Init(void)
{
    // v10.1 : make sure that these variables will survive even after optimization
    ezDSP_Version_SCI = VER_OF_THIS_FILE;
    ezDSP_uRead16BPossible = 1;
    ezDSP_uRead8BPossible = 1;

    #if defined(CPU1)
    ezDSP_uCPU = 1;
    #elif defined(CPU2)
    ezDSP_uCPU = 2;
    #endif


#if F28001x || F28002x || F28003x || F28004x || F28P55xS || F28E12x
    // GPIO28 is the SCI Rx pin.
    #if F28004x
    GPIO_setMasterCore(28, GPIO_CORE_CPU1);
    #endif
    #if F28003x
    GPIO_setPinConfig(GPIO_28_SCIA_RX);
    GPIO_setDirectionMode(28, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(28, GPIO_PIN_TYPE_PULLUP);
    GPIO_setQualificationMode(28, GPIO_QUAL_ASYNC);
    #elif F28001x
    GPIO_setPinConfig(DEVICE_GPIO_CFG_SCIRXDA);
    GPIO_setDirectionMode(DEVICE_GPIO_PIN_SCIRXDA, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(DEVICE_GPIO_PIN_SCIRXDA, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(DEVICE_GPIO_PIN_SCIRXDA, GPIO_QUAL_ASYNC);
    #else
    GPIO_setPinConfig(GPIO_28_SCIRXDA);
    GPIO_setDirectionMode(28, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(28, GPIO_PIN_TYPE_PULLUP);
    GPIO_setQualificationMode(28, GPIO_QUAL_ASYNC);
    #endif

    // GPIO29 is the SCI Tx pin.
    #if F28004x
    GPIO_setMasterCore(29, GPIO_CORE_CPU1);
    #endif
    #if F28003x
    GPIO_setPinConfig(GPIO_29_SCIA_TX);
    GPIO_setDirectionMode(29, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(29, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(29, GPIO_QUAL_ASYNC);
    #elif F28001x
    GPIO_setPinConfig(DEVICE_GPIO_CFG_SCITXDA);
    GPIO_setDirectionMode(DEVICE_GPIO_PIN_SCITXDA, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(DEVICE_GPIO_PIN_SCITXDA, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(DEVICE_GPIO_PIN_SCITXDA, GPIO_QUAL_ASYNC);
    #else
    GPIO_setPinConfig(GPIO_29_SCITXDA);
    GPIO_setDirectionMode(29, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(29, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(29, GPIO_QUAL_ASYNC);
    #endif
#elif F2807x || F2837xS || F2837xD_CPU1
    // GPIO85 is the SCI Rx pin.
    GPIO_setMasterCore(85, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_85_SCIRXDA);
    GPIO_setDirectionMode(85, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(85, GPIO_PIN_TYPE_PULLUP);
    GPIO_setQualificationMode(85, GPIO_QUAL_ASYNC);

    // GPIO84 is the SCI Tx pin.
    GPIO_setMasterCore(84, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_84_SCITXDA);
    GPIO_setDirectionMode(84, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(84, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(84, GPIO_QUAL_ASYNC);
#elif F2837xD_CPU1_CPU2
#ifdef CPU1
    // GPIO85 is the SCI Rx pin.
    GPIO_setMasterCore(85, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_85_SCIRXDA);
    GPIO_setDirectionMode(85, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(85, GPIO_PIN_TYPE_PULLUP);
    GPIO_setQualificationMode(85, GPIO_QUAL_ASYNC);

    // GPIO84 is the SCI Tx pin.
    GPIO_setMasterCore(84, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_84_SCITXDA);
    GPIO_setDirectionMode(84, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(84, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(84, GPIO_QUAL_ASYNC);

    // SCI-B connected to CPU2
    EALLOW;
    HWREG(CPUSYS_BASE + SYSCTL_O_PCLKCR7) &= ~SYSCTL_PCLKCR7_SCI_B;
    HWREG(DEVCFG_BASE + SYSCTL_O_CPUSEL5) |= SYSCTL_CPUSEL5_SCI_B;  // 0 = CPU1, 1 = CPU2 : This register must be configured prior to enabling the peripheral clocks
    HWREG(CPUSYS_BASE + SYSCTL_O_PCLKCR7) |= SYSCTL_PCLKCR7_SCI_B;
    EDIS;

    ///////////////////////////////////////////////////////////
    // modify below to change GPIOx for SCI-B
    ///////////////////////////////////////////////////////////
    // GPIO87 is the SCI-B Rx pin for CPU2.
    GPIO_setMasterCore(87, GPIO_CORE_CPU2);
    GPIO_setPinConfig(GPIO_87_SCIRXDB);
    GPIO_setDirectionMode(87, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(87, GPIO_PIN_TYPE_PULLUP);
    GPIO_setQualificationMode(87, GPIO_QUAL_ASYNC);

    // GPIO86 is the SCI-B Tx pin for CPU2.
    GPIO_setMasterCore(86, GPIO_CORE_CPU2);
    GPIO_setPinConfig(GPIO_86_SCITXDB);
    GPIO_setDirectionMode(86, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(86, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(86, GPIO_QUAL_ASYNC);
#endif
#elif F28P65xS || F28P65xD_CPU1
    // GPIO 13 is the SCI Rx pin.
    // GPIO 12 is the SCI Tx pin.
    #define EZ_DEVICE_GPIO_PIN_SCIRXDA     13U             // GPIO number for SCI RX
    #define EZ_DEVICE_GPIO_PIN_SCITXDA     12U             // GPIO number for SCI TX
    #define EZ_DEVICE_GPIO_CFG_SCIRXDA     GPIO_13_SCIA_RX // "pinConfig" for SCI RX
    #define EZ_DEVICE_GPIO_CFG_SCITXDA     GPIO_12_SCIA_TX // "pinConfig" for SCI TX

    GPIO_setControllerCore(EZ_DEVICE_GPIO_PIN_SCIRXDA, GPIO_CORE_CPU1);
    GPIO_setPinConfig(EZ_DEVICE_GPIO_CFG_SCIRXDA);
    GPIO_setDirectionMode(EZ_DEVICE_GPIO_PIN_SCIRXDA, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(EZ_DEVICE_GPIO_PIN_SCIRXDA, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(EZ_DEVICE_GPIO_PIN_SCIRXDA, GPIO_QUAL_ASYNC);

    GPIO_setControllerCore(EZ_DEVICE_GPIO_PIN_SCITXDA, GPIO_CORE_CPU1);
    GPIO_setPinConfig(EZ_DEVICE_GPIO_CFG_SCITXDA);
    GPIO_setDirectionMode(EZ_DEVICE_GPIO_PIN_SCITXDA, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(EZ_DEVICE_GPIO_PIN_SCITXDA, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(EZ_DEVICE_GPIO_PIN_SCITXDA, GPIO_QUAL_ASYNC);
#elif F28P65xD_CPU1_CPU2
#ifdef CPU1
    // GPIO 13 is the SCI Rx pin.
    // GPIO 12 is the SCI Tx pin.
    #define EZ_DEVICE_GPIO_PIN_SCIRXDA     13U             // GPIO number for SCI RX
    #define EZ_DEVICE_GPIO_PIN_SCITXDA     12U             // GPIO number for SCI TX
    #define EZ_DEVICE_GPIO_CFG_SCIRXDA     GPIO_13_SCIA_RX // "pinConfig" for SCI RX
    #define EZ_DEVICE_GPIO_CFG_SCITXDA     GPIO_12_SCIA_TX // "pinConfig" for SCI TX

    GPIO_setControllerCore(EZ_DEVICE_GPIO_PIN_SCIRXDA, GPIO_CORE_CPU1);
    GPIO_setPinConfig(EZ_DEVICE_GPIO_CFG_SCIRXDA);
    GPIO_setDirectionMode(EZ_DEVICE_GPIO_PIN_SCIRXDA, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(EZ_DEVICE_GPIO_PIN_SCIRXDA, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(EZ_DEVICE_GPIO_PIN_SCIRXDA, GPIO_QUAL_ASYNC);

    GPIO_setControllerCore(EZ_DEVICE_GPIO_PIN_SCITXDA, GPIO_CORE_CPU1);
    GPIO_setPinConfig(EZ_DEVICE_GPIO_CFG_SCITXDA);
    GPIO_setDirectionMode(EZ_DEVICE_GPIO_PIN_SCITXDA, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(EZ_DEVICE_GPIO_PIN_SCITXDA, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(EZ_DEVICE_GPIO_PIN_SCITXDA, GPIO_QUAL_ASYNC);

    // SCI-B connected to CPU2
    EALLOW;
    HWREG(CPUSYS_BASE + SYSCTL_O_PCLKCR7) &= ~SYSCTL_PCLKCR7_SCI_B;
    HWREG(DEVCFG_BASE + SYSCTL_O_CPUSEL5) |= SYSCTL_CPUSEL5_SCI_B;  // 0 = CPU1, 1 = CPU2 : This register must be configured prior to enabling the peripheral clocks
    HWREG(CPUSYS_BASE + SYSCTL_O_PCLKCR7) |= SYSCTL_PCLKCR7_SCI_B;
    EDIS;

    ///////////////////////////////////////////////////////////
    // modify below to change GPIOx for SCI-B
    ///////////////////////////////////////////////////////////
    // GPIO 87 is the SCI-B Rx pin.
    // GPIO 86 is the SCI-B Tx pin.
    #define EZ_DEVICE_GPIO_PIN_SCIRXDB     87U
    #define EZ_DEVICE_GPIO_PIN_SCITXDB     86U
    #define EZ_DEVICE_GPIO_CFG_SCIRXDB     GPIO_87_SCIB_RX
    #define EZ_DEVICE_GPIO_CFG_SCITXDB     GPIO_86_SCIB_TX

    GPIO_setControllerCore(EZ_DEVICE_GPIO_PIN_SCIRXDB, GPIO_CORE_CPU2);
    GPIO_setPinConfig(EZ_DEVICE_GPIO_CFG_SCIRXDB);
    GPIO_setDirectionMode(EZ_DEVICE_GPIO_PIN_SCIRXDB, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(EZ_DEVICE_GPIO_PIN_SCIRXDB, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(EZ_DEVICE_GPIO_PIN_SCIRXDB, GPIO_QUAL_ASYNC);

    GPIO_setControllerCore(EZ_DEVICE_GPIO_PIN_SCITXDB, GPIO_CORE_CPU2);
    GPIO_setPinConfig(EZ_DEVICE_GPIO_CFG_SCITXDB);
    GPIO_setDirectionMode(EZ_DEVICE_GPIO_PIN_SCITXDB, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(EZ_DEVICE_GPIO_PIN_SCITXDB, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(EZ_DEVICE_GPIO_PIN_SCITXDB, GPIO_QUAL_ASYNC);
#endif
#elif F2838xX_ALL
#ifdef CPU1
    // SCI-A connected to CPU1
    EALLOW;
    HWREG(CPUSYS_BASE + SYSCTL_O_PCLKCR7) &= ~SYSCTL_PCLKCR7_SCI_B;
    HWREG(DEVCFG_BASE + SYSCTL_O_CPUSEL5) &= ~SYSCTL_CPUSEL5_SCI_A;
    HWREG(CPUSYS_BASE + SYSCTL_O_PCLKCR7) |= SYSCTL_PCLKCR7_SCI_B;
    EDIS;

    // GPIO28 is the SCI Rx pin.
    GPIO_setMasterCore(28, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_28_SCIRXDA);
    GPIO_setDirectionMode(28, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(28, GPIO_PIN_TYPE_PULLUP);
    GPIO_setQualificationMode(28, GPIO_QUAL_ASYNC);

    // GPIO29 is the SCI Tx pin.
    GPIO_setMasterCore(29, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_29_SCITXDA);
    GPIO_setDirectionMode(29, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(29, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(29, GPIO_QUAL_ASYNC);

// note : change this #if block if another GPIO channel and port is required for CPU2
#if F2838xD_CPU1_CPU2 || F2838xD_CPU1_CPU2_CM
    // SCI-B connected to CPU2
    EALLOW;
    HWREG(CPUSYS_BASE + SYSCTL_O_PCLKCR7) &= ~SYSCTL_PCLKCR7_SCI_B;
    HWREG(DEVCFG_BASE + SYSCTL_O_CPUSEL5) |= SYSCTL_CPUSEL5_SCI_B;
    HWREG(CPUSYS_BASE + SYSCTL_O_PCLKCR7) |= SYSCTL_PCLKCR7_SCI_B;
    EDIS;
    ///////////////////////////////////////////////////////////
    // modify below to change GPIOx for SCI-B
    ///////////////////////////////////////////////////////////
    // GPIO15 is the SCI-B Rx pin for CPU2.
    GPIO_setMasterCore(15, GPIO_CORE_CPU2);
    GPIO_setPinConfig(GPIO_15_SCIRXDB);
    GPIO_setDirectionMode(15, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(15, GPIO_PIN_TYPE_PULLUP);
    GPIO_setQualificationMode(15, GPIO_QUAL_ASYNC);
    // GPIO14 is the SCI-B Tx pin for CPU2.
    GPIO_setMasterCore(14, GPIO_CORE_CPU2);
    GPIO_setPinConfig(GPIO_14_SCITXDB);
    GPIO_setDirectionMode(14, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(14, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(14, GPIO_QUAL_ASYNC);
#endif

    // note : change this #if block if another GPIO port is required for CM UART-0
#if F2838xX_CPU1_CM || F2838xD_CPU1_CPU2_CM
    ///////////////////////////////////////////////////////////
    // modify below to change GPIOx for UART-0
    ///////////////////////////////////////////////////////////
    // GPIO85 is the UART-A Rx pin for CM
    GPIO_setMasterCore(85, GPIO_CORE_CM);
    GPIO_setPinConfig(GPIO_85_CMUARTRXA);
    GPIO_setDirectionMode(85, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(85, GPIO_PIN_TYPE_PULLUP);
    GPIO_setQualificationMode(85, GPIO_QUAL_ASYNC);
    // GPIO84 is the UART-A Tx pin for CM
    GPIO_setMasterCore(84, GPIO_CORE_CM);
    GPIO_setPinConfig(GPIO_84_CMUARTTXA);
    GPIO_setDirectionMode(84, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(84, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(84, GPIO_QUAL_ASYNC);
#endif
#endif

#else   // MCU is not defined
    this will create compiler error intentionally
#endif

    // Map the ISR to the wake interrupt.
    if(SCIx_BASE == SCIA_BASE) {
        Interrupt_register(INT_SCIA_RX, easy_RXINT_ISR);
        // Enable the SCI clocks
        #if F28E12x
        SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);    
        #else
        SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_SCIA);
        #endif
    }
    #if !F28002x
    else if(SCIx_BASE == SCIB_BASE) {
        Interrupt_register(INT_SCIB_RX, easy_RXINT_ISR);
        #if F28E12x
        SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);    
        #else
        SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_SCIA);
        #endif
    }
    #endif

    // Configure SCI
    SCI_setConfig(SCIx_BASE, DEVICE_LSPCLK_FREQ, BAUDRATE, (SCI_CONFIG_WLEN_8 |
                                            SCI_CONFIG_STOP_ONE |
                                             SCI_CONFIG_PAR_NONE));
    SCI_enableModule(SCIx_BASE);
    //SCI_enableLoopback(SCIx_BASE);
    SCI_resetChannels(SCIx_BASE);
    SCI_enableFIFO(SCIx_BASE);

    // RX and TX FIFO Interrupts Enabled
    SCI_enableInterrupt(SCIx_BASE, SCI_INT_RXFF);
    SCI_disableInterrupt(SCIx_BASE, SCI_INT_RXERR);

    SCI_setFIFOInterruptLevel(SCIx_BASE, SCI_FIFO_TX1, SCI_FIFO_RX1);
    SCI_performSoftwareReset(SCIx_BASE);

    SCI_resetTxFIFO(SCIx_BASE);
    SCI_resetRxFIFO(SCIx_BASE);

    // Enable the interrupts in the PIE: Group 9 interrupts 1 & 2.
    if(SCIx_BASE == SCIA_BASE)      Interrupt_enable(INT_SCIA_RX);
    #if !F28002x
    else if(SCIx_BASE == SCIB_BASE) Interrupt_enable(INT_SCIB_RX);
    #endif

    #if F28E12x
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP7);
    #else
	// It's same interrupt group between SCI-A and SCI-B. 
	// But need to modify in case you use another SCI-x instead of SCI-B
	Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP9);
    #endif

    // Enable Global Interrupt (INTM) and realtime interrupt (DBGM)
    EINT;
    ERTM;

    // others
#ifdef _FLASH
    ezDSP_uOnChipFlash = 1;
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// common
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(FLASH_API_WRAPPER) || (defined(CPU1) && (F28P65xD_CPU1_CPU2 || F2838xD_CPU1_CPU2 || F2838xX_CPU1_CM || F2838xD_CPU1_CPU2_CM))
/////////////////////////////////////////////////////////////////////////////////
// auto bauding
/////////////////////////////////////////////////////////////////////////////////
void easyDSP_AutoBaud(void)
{
    uint16_t byteData;
    /////////////////////////////////////////////////////////////////////////////
    // Initialize the SCI-A port for communications with the host.
    /////////////////////////////////////////////////////////////////////////////
    // Enable the SCI-A clocks
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_SCIA);      // called in CM_init()
    SysCtl_setLowSpeedClock(SYSCTL_LSPCLK_PRESCALE_4);    // called in CM_init()

    EALLOW;
    HWREGH(SCIA_BASE + SCI_O_FFTX) = SCI_FFTX_SCIRST;
    // 1 stop bit, No parity, 8-bit character, No loopback
    HWREGH(SCIA_BASE + SCI_O_CCR) = (SCI_CONFIG_WLEN_8 | SCI_CONFIG_STOP_ONE);
    SCI_setParityMode(SCIA_BASE, SCI_CONFIG_PAR_NONE);
    // Enable TX, RX, Use internal SCICLK
    HWREGH(SCIA_BASE + SCI_O_CTL1) = (SCI_CTL1_TXENA | SCI_CTL1_RXENA);
    // Disable RxErr, Sleep, TX Wake, Rx Interrupt, Tx Interrupt
    HWREGB(SCIA_BASE + SCI_O_CTL2) = 0x0U;
    // Relinquish SCI-A from reset and enable TX/RX
    SCI_enableModule(SCIA_BASE);
    EDIS;
    /////////////////////////////////////////////////////////////////////////////
    // Perform autobaud lock with the host.
    /////////////////////////////////////////////////////////////////////////////
    SCI_lockAutobaud(SCIA_BASE);
    // Read data
    byteData = SCI_readCharBlockingNonFIFO(SCIA_BASE);
    // Configure TX pin after autobaud lock (Performed here to allow SCI to double as wait boot)
    // If below two lines are allowed,  Tx signal gets low short time in the beginning. refer to excel tech document.
    //GPIO_setPadConfig(29, GPIO_PIN_TYPE_STD);
    //GPIO_setPinConfig(GPIO_29_SCITXDA);
    // Write data to request key
    SCI_writeCharNonBlocking(SCIA_BASE, byteData);
}
#endif

/////////////////////////////////////////////////////////////////////////////////
// boot and sync for multi-core MCU
// note : For 2838xD and 28P65xD, you can modify flash sector location if sector 0 is not preferred
/////////////////////////////////////////////////////////////////////////////////
#if F28P65xD_CPU1_CPU2 || F2837xD_CPU1_CPU2 || F2838xS_CPU1_CM || F2838xD_CPU1_CM || F2838xD_CPU1_CPU2 || F2838xD_CPU1_CPU2_CM
// boot and synch for multi-core MCU like 28P65xD, 2837xD and 2838x
// note : For 28P65x and 2838x, you can modify flash sector location if sector 0 is not preferred
void easyDSP_Boot_Sync(void)
{
    // flash booting in this function is not any more recommended from v11 onwards.
    // please place the below code directly to main.c.
    // But keep the contents for backward compatibility for now.
#ifdef _FLASH
#ifdef CPU1
#if F2837xD_CPU1_CPU2
    easyDSP_FlashBootCPU2();
#elif F28P65xD_CPU1_CPU2
    Device_bootCPU2(BOOTMODE_BOOT_TO_FLASH_BANK3_SECTOR0);
#elif F2838xD_CPU1_CPU2 || F2838xD_CPU1_CPU2_CM
    // for CPU2 flash booting
    Device_bootCPU2(BOOTMODE_BOOT_TO_FLASH_SECTOR0);
    //Device_bootCPU2(BOOTMODE_BOOT_TO_FLASH_SECTOR4);  // other options
    //Device_bootCPU2(BOOTMODE_BOOT_TO_FLASH_SECTOR8);
    //Device_bootCPU2(BOOTMODE_BOOT_TO_FLASH_SECTOR13);
#endif
    // v11 bug fix
#if F2838xS_CPU1_CM || F2838xD_CPU1_CM || F2838xD_CPU1_CPU2_CM
    // for CM flash booting
    Device_bootCM(BOOTMODE_BOOT_TO_FLASH_SECTOR0);
    //Device_bootCM(BOOTMODE_BOOT_TO_FLASH_SECTOR4);    // other options
    //Device_bootCM(BOOTMODE_BOOT_TO_FLASH_SECTOR8);
    //Device_bootCM(BOOTMODE_BOOT_TO_FLASH_SECTOR13);
#endif
#endif // CPU1
#else   // if not flash
#if defined(CPU1) || defined(CPU2)
#if F2837xD_CPU1_CPU2
    easyDSP_SCIBootCPU2();
#elif F2838xD_CPU1_CPU2 || F2838xD_CPU1_CM || F2838xS_CPU1_CM || F2838xD_CPU1_CPU2_CM
    easyDSP_2838x_Sci_Boot_Sync();
#elif F28P65xD_CPU1_CPU2
    easyDSP_28P65x_Sci_Boot_Sync();
#else
    sanity check
#endif
#endif  // CPU1 and CPU2
#endif  // _FLASH
}
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// F2838x multi core boot operation
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef _FLASH
#if F2838xD_CPU1_CPU2 || F2838xX_CPU1_CM || F2838xD_CPU1_CPU2_CM
#define CPU1_CPU2_SYNC_FLAG         IPC_FLAG5
#define CPU1_CPU2_2ND_SYNC_FLAG     IPC_FLAG6
#define CPU1_CM_SYNC_FLAG           IPC_FLAG31
#define CPU1_CM_COMM_FLAG           IPC_FLAG30
void easyDSP_2838x_Sci_Boot_Sync(void)
{

#ifdef CPU1
#if F2838xD_CPU1_CPU2 || F2838xD_CPU1_CPU2_CM
    easyDSP_SCIBootCPU2();
#endif
#if F2838xX_CPU1_CM || F2838xD_CPU1_CPU2_CM
    easyDSP_SCIBootCM();
#endif
#if F2838xD_CPU1_CPU2_CM
    IPC_sync(IPC_CPU1_L_CPU2_R, CPU1_CPU2_2ND_SYNC_FLAG);   // second sync with CPU2 after CM boot
#endif
#endif  // CPU1
#ifdef CPU2
#if F2838xD_CPU1_CPU2 || F2838xD_CPU1_CPU2_CM
    easyDSP_SCIBootCPU2();
    DEVICE_DELAY_US(1000);    // Wait until CPU1 is configuring SCI-B for CPU2 in easyDSP_SCI_Init()
#endif
#if F2838xD_CPU1_CPU2_CM
    IPC_sync(IPC_CPU2_L_CPU1_R, CPU1_CPU2_2ND_SYNC_FLAG);   // second sync with CPU1 after CM boot
    DEVICE_DELAY_US(1000);    // Wait until CPU1 is configuring SCI-B for CPU2 in easyDSP_SCI_Init()
#endif
#endif  // CPU2
}

#ifdef CPU1
uint32_t ezDSP_u32CPU2BlockWordSize = 0, ezDSP_u32CMBlockWordSize = 0;  // only for monitoring
uint32_t ezDSP_u32CPU2BootStatus = 0, ezDSP_u32CMBootStatus = 0;
uint16_t easyDSP_GetWordData(void)
{
    uint16_t wordData, byteData;
    wordData = byteData = 0x0000;

    // Fetch the LSB and verify back to the host
    while(!SCI_isDataAvailableNonFIFO(SCIA_BASE));
    wordData =  SCI_readCharNonBlocking(SCIA_BASE);
    SCI_writeCharNonBlocking(SCIA_BASE, wordData);

    // Fetch the MSB and verify back to the host
    while(!SCI_isDataAvailableNonFIFO(SCIA_BASE));
    byteData =  SCI_readCharNonBlocking(SCIA_BASE);
    SCI_writeCharNonBlocking(SCIA_BASE, byteData);

    // form the wordData from the MSB:LSB
    wordData |= (byteData << 8);

    return wordData;
}

uint32_t easyDSP_GetLongData(void)
{
    uint32_t longData;
    longData = ( (uint32_t)easyDSP_GetWordData() << 16);    // Fetch the upper 1/2 of the 32-bit value
    longData |= (uint32_t)easyDSP_GetWordData();            // Fetch the lower 1/2 of the 32-bit value
    return longData;
}

void easyDSP_CopyData(uint16_t uMCU)
{
    struct HEADER {
        uint32_t DestAddr;
        uint16_t BlockSize;
    } BlockHeader;

    uint16_t wordData;
    uint16_t i;

   BlockHeader.BlockSize = easyDSP_GetWordData();
   if(uMCU == 3) BlockHeader.BlockSize /= 2;

   // monitoring
   if(uMCU == 2)        ezDSP_u32CPU2BlockWordSize    += BlockHeader.BlockSize;
   else if(uMCU == 3)   ezDSP_u32CMBlockWordSize      += BlockHeader.BlockSize;

   while(BlockHeader.BlockSize != 0)
   {
      BlockHeader.DestAddr = easyDSP_GetLongData();
      if(uMCU == 2) {
          BlockHeader.DestAddr -= 0x400;
          BlockHeader.DestAddr += CPU1TOCPU2MSGRAM1_BASE;
      }
      else if(uMCU == 3) {
          BlockHeader.DestAddr -= 0x20000800;
          BlockHeader.DestAddr /= 2;    // CM address
          BlockHeader.DestAddr += CPUXTOCMMSGRAM1_BASE;
      }

      for(i = 0; i < BlockHeader.BlockSize; i++) {
          wordData = easyDSP_GetWordData();
          if(uMCU == 2)         *(uint16_t *)BlockHeader.DestAddr = wordData;
          else if(uMCU == 3)    *(uint16_t *)BlockHeader.DestAddr = (wordData >> 8) | (wordData << 8);
          BlockHeader.DestAddr++;
      }

      BlockHeader.BlockSize = easyDSP_GetWordData();
      if(uMCU == 3) BlockHeader.BlockSize /= 2;

      // monitoring
      if(uMCU == 2)        ezDSP_u32CPU2BlockWordSize += BlockHeader.BlockSize;
      else if(uMCU == 3)   ezDSP_u32CMBlockWordSize += BlockHeader.BlockSize;
   }
}

#if F2838xD_CPU1_CPU2 || F2838xD_CPU1_CPU2_CM
void easyDSP_SCIBootCPU2(void)
{
    // GPIO28 is the SCI Rx pin.
    GPIO_setMasterCore(28, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_28_SCIRXDA);
    GPIO_setDirectionMode(28, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(28, GPIO_PIN_TYPE_PULLUP);
    GPIO_setQualificationMode(28, GPIO_QUAL_ASYNC);

    // GPIO29 is the SCI Tx pin.
    GPIO_setMasterCore(29, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_29_SCITXDA);
    GPIO_setDirectionMode(29, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(29, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(29, GPIO_QUAL_ASYNC);

    // copy SCI booting agent code to CPU1TOCPU2MSGRAM1
    uint16_t i;
    easyDSP_AutoBaud();
    if(easyDSP_GetWordData() != 0x08AA) while(1);   // KeyValue
    for(i = 0; i < 8; i++) easyDSP_GetWordData();   // 8 reserved words
    easyDSP_GetWordData();                          // entry point
    easyDSP_GetWordData();                          // entry point
    easyDSP_CopyData(2);

    // secure the last echo byte
    DEVICE_DELAY_US(1000*10);

    // SCIA connected to CPU2
    EALLOW;
    HWREG(CPUSYS_BASE + SYSCTL_O_PCLKCR7) &= ~SYSCTL_PCLKCR7_SCI_A;
    HWREG(DEVCFG_BASE + SYSCTL_O_CPUSEL5) |= SYSCTL_CPUSEL5_SCI_A;
    HWREG(CPUSYS_BASE + SYSCTL_O_PCLKCR7) |= SYSCTL_PCLKCR7_SCI_A;
    EDIS;

    //Allows CPU02 bootrom to take control of clock configuration registers
    EALLOW;
    HWREG(CLKCFG_BASE + SYSCTL_O_CLKSEM) = 0xA5A50000;
    HWREG(CLKCFG_BASE + SYSCTL_O_LOSPCP) = 0x0002;
    EDIS;

    // GPIO28 SCIA-RX to CPU2
    GPIO_setMasterCore(28, GPIO_CORE_CPU2);
    GPIO_setPinConfig(GPIO_28_SCIRXDA);
    GPIO_setDirectionMode(28, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(28, GPIO_PIN_TYPE_PULLUP);
    GPIO_setQualificationMode(28, GPIO_QUAL_ASYNC);

    // GPIO29 SCIA-TX to CPU2
    GPIO_setMasterCore(29, GPIO_CORE_CPU2);
    GPIO_setPinConfig(GPIO_29_SCITXDA);
    GPIO_setDirectionMode(29, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(29, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(29, GPIO_QUAL_ASYNC);

#if 0   // v11
    // Assign all GS RAMs to CPU2
    // NOTE : don't assign GSxRAM to CPU2 where .text of CPU1 is located !!!!!!!
    EALLOW;
    HWREG(MEMCFG_BASE + MEMCFG_O_GSXMSEL) = 0xFFFF;
    EDIS;
#endif

    // Boot CPU2 /w agent. max. (1000) copy
    IPC_Instance[IPC_CPU1_L_CPU2_R].IPC_Boot_Pump_Reg->IPC_BOOTMODE = (BOOT_KEY | CPU2_BOOT_FREQ_200MHZ | BOOTMODE_IPC_MSGRAM_COPY_LENGTH_1000W | BOOTMODE_IPC_MSGRAM_COPY_BOOT_TO_M1RAM);
    IPC_Instance[IPC_CPU1_L_CPU2_R].IPC_Flag_Ctr_Reg->IPC_SET = IPC_FLAG0;
    uint32_t clearvalue;
    EALLOW;
    clearvalue = HWREG(DEVCFG_BASE + SYSCTL_O_CPU2RESCTL);
    clearvalue &= ~SYSCTL_CPU2RESCTL_RESET;
    HWREG(DEVCFG_BASE + SYSCTL_O_CPU2RESCTL) = (SYSCTL_REG_KEY & SYSCTL_CPU2RESCTL_KEY_M) | clearvalue;
    EDIS;
    while((HWREGH(DEVCFG_BASE + SYSCTL_O_RSTSTAT) &
            SYSCTL_RSTSTAT_CPU2RES) == 0U);

    DEVICE_DELAY_US(1000*1);    // wait CPU2 boot
    ezDSP_u32CPU2BootStatus = IPC_Instance[IPC_CPU1_L_CPU2_R].IPC_Boot_Pump_Reg->IPC_BOOTSTS;

    ///////////////////////////////////////////////////////////////////////////////////
    // download target CPU2 code via SCI-A. This is done by agent program in CPU2
    ///////////////////////////////////////////////////////////////////////////////////

    // Synchronize both the cores : wait until CPU2 is booted and set CPU1_CPU2_SYNC_FLAG
    IPC_Instance[IPC_CPU1_L_CPU2_R].IPC_Flag_Ctr_Reg->IPC_SET = CPU1_CPU2_SYNC_FLAG;
    while((IPC_Instance[IPC_CPU1_L_CPU2_R].IPC_Flag_Ctr_Reg->IPC_STS & CPU1_CPU2_SYNC_FLAG) == 0U);
    IPC_Instance[IPC_CPU1_L_CPU2_R].IPC_Flag_Ctr_Reg->IPC_ACK = CPU1_CPU2_SYNC_FLAG;
    while((IPC_Instance[IPC_CPU1_L_CPU2_R].IPC_Flag_Ctr_Reg->IPC_FLG & CPU1_CPU2_SYNC_FLAG) != 0U);

#if 0 // v11
    // Assign all GS RAMs to CPU1
    EALLOW;
    HWREG(MEMCFG_BASE + MEMCFG_O_GSXMSEL) = 0x0000;
    EDIS;
#endif

    // SCIA connected to CPU1
    EALLOW;
    HWREG(CPUSYS_BASE + SYSCTL_O_PCLKCR7) &= ~SYSCTL_PCLKCR7_SCI_A;
    HWREG(DEVCFG_BASE + SYSCTL_O_CPUSEL5) &= ~SYSCTL_CPUSEL5_SCI_A;
    HWREG(CPUSYS_BASE + SYSCTL_O_PCLKCR7) |= SYSCTL_PCLKCR7_SCI_A;
    EDIS;
}
#endif // F2838xD_CPU1_CPU2 || F2838xD_CPU1_CPU2_CM


#if F2838xX_CPU1_CM || F2838xD_CPU1_CPU2_CM
void easyDSP_SendWordToCM(uint16_t wordData)
{
    IPC_sendResponse(IPC_CPU1_L_CM_R, wordData);
    IPC_sync(IPC_CPU1_L_CM_R, CPU1_CM_COMM_FLAG);
    IPC_sync(IPC_CPU1_L_CM_R, CPU1_CM_COMM_FLAG);
}

void easyDSP_SendAppToCM(void)
{
    struct HEADER {
        uint32_t DestAddr;
        uint16_t BlockSize;
    } BlockHeader;
    uint16_t wordData;
    uint16_t i;

    BlockHeader.BlockSize = easyDSP_GetWordData();
    BlockHeader.BlockSize /= 2; // byte to word
    easyDSP_SendWordToCM(BlockHeader.BlockSize);

    while(BlockHeader.BlockSize != 0)
    {
        BlockHeader.DestAddr = easyDSP_GetLongData();
        easyDSP_SendWordToCM((uint16_t)(BlockHeader.DestAddr >> 16));   // MSW
        easyDSP_SendWordToCM((uint16_t)BlockHeader.DestAddr);           // LSW

        for(i = 1; i <= BlockHeader.BlockSize; i++) {
            wordData = easyDSP_GetWordData();
            easyDSP_SendWordToCM(wordData);
        }
        BlockHeader.BlockSize = easyDSP_GetWordData();
        BlockHeader.BlockSize /= 2;
        easyDSP_SendWordToCM(BlockHeader.BlockSize);
    }
}

bool ezDSP_bIsCMResetBeforeBooting;
void easyDSP_SCIBootCM(void)
{
    uint16_t i;

    // GPIO28 is the SCI Rx pin.
    GPIO_setMasterCore(28, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_28_SCIRXDA);
    GPIO_setDirectionMode(28, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(28, GPIO_PIN_TYPE_PULLUP);
    GPIO_setQualificationMode(28, GPIO_QUAL_ASYNC);

    // GPIO29 is the SCI Tx pin.
    GPIO_setMasterCore(29, GPIO_CORE_CPU1);
    GPIO_setPinConfig(GPIO_29_SCITXDA);
    GPIO_setDirectionMode(29, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(29, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(29, GPIO_QUAL_ASYNC);

    // clear message ram for test purpose
    //for(i = 0; i < (1000/2); i++) HWREG(CPUXTOCMMSGRAM1_BASE + i) = 0;  // 1000x16bit = 500x32bit.

    // copy SCI booting agent code to CPU1TOCMMSGRAM1
    easyDSP_AutoBaud();
    if(easyDSP_GetWordData() != 0x08AA) while(1);   // KeyValue
    for(i = 0; i < 8; i++) easyDSP_GetWordData();   // 8 reserved words
    easyDSP_GetWordData();                          // entry point
    easyDSP_GetWordData();                          // entry point
    easyDSP_CopyData(3);

    // secure the last echo
    DEVICE_DELAY_US(1000*1);

    // CM should be in reset
    ezDSP_bIsCMResetBeforeBooting = SysCtl_isCMReset();

    // Boot CM /w agent: copy max. size (1000 word)
    Device_bootCM(BOOTMODE_IPC_MSGRAM_COPY_LENGTH_1000W | BOOTMODE_IPC_MSGRAM_COPY_BOOT_TO_S0RAM);
    DEVICE_DELAY_US(1000*1);    // wait CM boot
    ezDSP_u32CMBootStatus = IPC_getBootStatus(IPC_CPU1_L_CM_R);

    // download target CM code via SCI-A. This is done by agent program in CM
    if(easyDSP_GetWordData() != 0x08AA) while(1);   // KeyValue
    for(i = 0; i < 8; i++) easyDSP_GetWordData();   // 8 reserved words
    easyDSP_SendWordToCM(easyDSP_GetWordData());    // entry point
    easyDSP_SendWordToCM(easyDSP_GetWordData());    // entry point
    easyDSP_SendAppToCM();
    // secure the last echo byte
    DEVICE_DELAY_US(1000*1);

    // Synchronize both the cores
    IPC_sync(IPC_CPU1_L_CM_R, CPU1_CM_SYNC_FLAG);
}
#endif  // F2838xX_CPU1_CM || F2838xD_CPU1_CPU2_CM
#endif  // CPU1

#ifdef CPU2
#if F2838xD_CPU1_CPU2 || F2838xD_CPU1_CPU2_CM
// easyDSP_SCIBootCPU2 for F2838xD CPU2
void easyDSP_SCIBootCPU2(void)
{
    // booting echo feedback still ongoing even this moment
    // So no switch SCIA channel to CPU1 immediately
    DEVICE_DELAY_US(1000*10);

    // Synchronize both the cores.
    IPC_sync(IPC_CPU2_L_CPU1_R, CPU1_CPU2_SYNC_FLAG);

    asm(" NOP");
    asm(" NOP");

    // Wait until CPU1 is configuring SCI-B for CPU2 in easyDSP_SCI_Init()
    //DEVICE_DELAY_US(1000*10);
}
#endif  // F2838xD_CPU1_CPU2 || F2838xD_CPU1_CPU2_CM
#endif  // CPU2
#endif  // F2838xD_CPU1_CPU2 || F2838xX_CPU1_CM || F2838xD_CPU1_CPU2_CM
#endif  // _FLASH

/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// F28P65x multi core boot operation for ram booting
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////

#if F28P65xD_CPU1_CPU2 && !defined(_FLASH)
#define CPU1_CPU2_SYNC_FLAG         IPC_FLAG5
void easyDSP_28P65x_Sci_Boot_Sync(void)
{
    easyDSP_SCIBootCPU2();
#ifdef CPU2
    DEVICE_DELAY_US(1000);    // Wait until CPU1 is configuring SCI-B for CPU2 in easyDSP_SCI_Init()
#endif
}

#ifdef CPU1
uint32_t ezDSP_u32CPU2BlockWordSize = 0, ezDSP_u32CMBlockWordSize = 0;  // only for monitoring
uint32_t ezDSP_u32CPU2BootStatus = 0, ezDSP_u32CMBootStatus = 0;
uint16_t easyDSP_GetWordData(void)
{
    uint16_t wordData, byteData;
    wordData = byteData = 0x0000;

    // Fetch the LSB and verify back to the host
    while(!SCI_isDataAvailableNonFIFO(SCIA_BASE));
    wordData =  SCI_readCharNonBlocking(SCIA_BASE);
    SCI_writeCharNonBlocking(SCIA_BASE, wordData);

    // Fetch the MSB and verify back to the host
    while(!SCI_isDataAvailableNonFIFO(SCIA_BASE));
    byteData =  SCI_readCharNonBlocking(SCIA_BASE);
    SCI_writeCharNonBlocking(SCIA_BASE, byteData);

    // form the wordData from the MSB:LSB
    wordData |= (byteData << 8);
    return wordData;
}

uint32_t easyDSP_GetLongData(void)
{
    uint32_t longData;
    longData = ( (uint32_t)easyDSP_GetWordData() << 16);    // Fetch the upper 1/2 of the 32-bit value
    longData |= (uint32_t)easyDSP_GetWordData();            // Fetch the lower 1/2 of the 32-bit value
    return longData;
}

void easyDSP_CopyData(void)
{
    struct HEADER {
        uint32_t DestAddr;
        uint16_t BlockSize;
    } BlockHeader;

    uint16_t wordData;
    uint16_t i;

    BlockHeader.BlockSize = easyDSP_GetWordData();

    // monitoring
    ezDSP_u32CPU2BlockWordSize += BlockHeader.BlockSize;

    while(BlockHeader.BlockSize != 0)
    {
        BlockHeader.DestAddr = easyDSP_GetLongData();
        BlockHeader.DestAddr -= 0x400;
        BlockHeader.DestAddr += CPU1TOCPU2MSGRAM0_BASE;

        for(i = 0; i < BlockHeader.BlockSize; i++) {
            wordData = easyDSP_GetWordData();
            *(uint16_t *)BlockHeader.DestAddr = wordData;
            BlockHeader.DestAddr++;
        }

        BlockHeader.BlockSize = easyDSP_GetWordData();

        // monitoring
        ezDSP_u32CPU2BlockWordSize += BlockHeader.BlockSize;
    }
}

void easyDSP_SCIBootCPU2(void)
{
    // GPIO 13 is the SCI Rx pin.
    // GPIO 12 is the SCI Tx pin.
    GPIO_setControllerCore(EZ_DEVICE_GPIO_PIN_SCIRXDA, GPIO_CORE_CPU1);
    GPIO_setPinConfig(EZ_DEVICE_GPIO_CFG_SCIRXDA);
    GPIO_setDirectionMode(EZ_DEVICE_GPIO_PIN_SCIRXDA, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(EZ_DEVICE_GPIO_PIN_SCIRXDA, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(EZ_DEVICE_GPIO_PIN_SCIRXDA, GPIO_QUAL_ASYNC);

    GPIO_setControllerCore(EZ_DEVICE_GPIO_PIN_SCITXDA, GPIO_CORE_CPU1);
    GPIO_setPinConfig(EZ_DEVICE_GPIO_CFG_SCITXDA);
    GPIO_setDirectionMode(EZ_DEVICE_GPIO_PIN_SCITXDA, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(EZ_DEVICE_GPIO_PIN_SCITXDA, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(EZ_DEVICE_GPIO_PIN_SCITXDA, GPIO_QUAL_ASYNC);


    // copy SCI booting agent code to CPU1TOCPU2MSGRAM0
    uint16_t i;
    easyDSP_AutoBaud();
    if(easyDSP_GetWordData() != 0x08AA) while(1);   // KeyValue
    for(i = 0; i < 8; i++) easyDSP_GetWordData();   // 8 reserved words
    easyDSP_GetWordData();                          // entry point
    easyDSP_GetWordData();                          // entry point
    easyDSP_CopyData();

    // secure the last echo byte
    DEVICE_DELAY_US(1000*10);

    // SCIA connected to CPU2
    EALLOW;
    HWREG(CPUSYS_BASE + SYSCTL_O_PCLKCR7) &= ~SYSCTL_PCLKCR7_SCI_A;
    HWREG(DEVCFG_BASE + SYSCTL_O_CPUSEL5) |= SYSCTL_CPUSEL5_SCI_A;
    HWREG(CPUSYS_BASE + SYSCTL_O_PCLKCR7) |= SYSCTL_PCLKCR7_SCI_A;
    EDIS;

    //Allows CPU02 bootrom to take control of clock configuration registers
    EALLOW;
    HWREG(CLKCFG_BASE + SYSCTL_O_CLKSEM) = 0xA5A50000;
    HWREG(CLKCFG_BASE + SYSCTL_O_LOSPCP) = 0x0002;
    EDIS;

    // SCIA to CPU2
    GPIO_setControllerCore(EZ_DEVICE_GPIO_PIN_SCIRXDA, GPIO_CORE_CPU2);
    GPIO_setControllerCore(EZ_DEVICE_GPIO_PIN_SCITXDA, GPIO_CORE_CPU2);

    // Boot CPU2 /w agent. max. (1000) copy
#if 0
    IPC_Instance[IPC_CPU1_L_CPU2_R].IPC_Boot_Pump_Reg->IPC_BOOTMODE = 0x5A0A0001;
    IPC_Instance[IPC_CPU1_L_CPU2_R].IPC_Flag_Ctr_Reg->IPC_SET = IPC_FLAG0;
    uint32_t clearvalue;
    EALLOW;
    clearvalue = HWREG(DEVCFG_BASE + SYSCTL_O_CPU2RESCTL);
    clearvalue &= ~SYSCTL_CPU2RESCTL_RESET;
    HWREG(DEVCFG_BASE + SYSCTL_O_CPU2RESCTL) = (SYSCTL_REG_KEY & SYSCTL_CPU2RESCTL_KEY_M) | clearvalue;
    EDIS;
    while((HWREGH(DEVCFG_BASE + SYSCTL_O_RSTSTAT) &
            SYSCTL_RSTSTAT_CPU2RES) == 0U);
#else
    IPC_setBootMode(IPC_CPU1_L_CPU2_R, 0x5A0A0001);     // Configure the CPU1TOCPU2IPCBOOTMODE register
    IPC_setFlagLtoR(IPC_CPU1_L_CPU2_R, IPC_FLAG0);      // Set IPC Flag 0
    SysCtl_controlCPU2Reset(SYSCTL_CORE_DEACTIVE);      // Bring CPU2 out of reset. Wait for CPU2 to go out of reset.
    while(SysCtl_isCPU2Reset() == 0x1U);
#endif

    DEVICE_DELAY_US(1000*1);    // wait CPU2 boot
    ezDSP_u32CPU2BootStatus = IPC_Instance[IPC_CPU1_L_CPU2_R].IPC_Boot_Pump_Reg->IPC_BOOTSTS;

    ///////////////////////////////////////////////////////////////////////////////////
    // download target CPU2 code via SCI-A. This is done by agent program in CPU2
    ///////////////////////////////////////////////////////////////////////////////////
    // Synchronize both the cores : wait until CPU2 is booted and set CPU1_CPU2_SYNC_FLAG
#if 0
    IPC_Instance[IPC_CPU1_L_CPU2_R].IPC_Flag_Ctr_Reg->IPC_SET = CPU1_CPU2_SYNC_FLAG;
    while((IPC_Instance[IPC_CPU1_L_CPU2_R].IPC_Flag_Ctr_Reg->IPC_STS & CPU1_CPU2_SYNC_FLAG) == 0U);
    IPC_Instance[IPC_CPU1_L_CPU2_R].IPC_Flag_Ctr_Reg->IPC_ACK = CPU1_CPU2_SYNC_FLAG;
    while((IPC_Instance[IPC_CPU1_L_CPU2_R].IPC_Flag_Ctr_Reg->IPC_FLG & CPU1_CPU2_SYNC_FLAG) != 0U);
#else
    IPC_sync(IPC_CPU1_L_CPU2_R, CPU1_CPU2_SYNC_FLAG);
#endif

    // SCIA connected to CPU1
    EALLOW;
    HWREG(CPUSYS_BASE + SYSCTL_O_PCLKCR7) &= ~SYSCTL_PCLKCR7_SCI_A;
    HWREG(DEVCFG_BASE + SYSCTL_O_CPUSEL5) &= ~SYSCTL_CPUSEL5_SCI_A;
    HWREG(CPUSYS_BASE + SYSCTL_O_PCLKCR7) |= SYSCTL_PCLKCR7_SCI_A;
    EDIS;
}
#endif  // CPU1

#ifdef CPU2
// easyDSP_SCIBootCPU2 for F2838xD CPU2
void easyDSP_SCIBootCPU2(void)
{
    // booting echo feedback still ongoing even this moment
    // So no switch SCIA channel to CPU1 immediately
    DEVICE_DELAY_US(1000*10);

    // Synchronize both the cores.
    IPC_sync(IPC_CPU2_L_CPU1_R, CPU1_CPU2_SYNC_FLAG);

    asm(" NOP");
    asm(" NOP");
}
#endif  // CPU2
#endif  // F28P65xD_CPU1_CPU2


/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// F2837x multi core boot operation
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////
#if F2837xD_CPU1_CPU2
#ifdef CPU1
#ifndef _FLASH
// Flags
#ifndef IPC_FLAG0
#define IPC_FLAG0                   0x00000001  // IPC FLAG 0
#define IPC_FLAG31                  0x80000000  // IPC FLAG 31
#endif
uint32_t ezDSP_u32C1BROMSTATUS = 0, ezDSP_u32BootStatus = 0;
void easyDSP_SCIBootCPU2(void)
{
    ezDSP_u32C1BROMSTATUS = HWREG(0x0000002C);

    // Loop until CPU02 control system IPC flags 1 and 32 are available
    while((HWREG(IPC_BASE + IPC_O_FLG) & IPC_FLAG0) || (HWREG(IPC_BASE + IPC_O_FLG) & IPC_FLAG31));

    // SCIA connected to CPU2
    EALLOW;
    HWREG(CPUSYS_BASE + SYSCTL_O_PCLKCR7) &= ~SYSCTL_PCLKCR7_SCI_A;
    HWREG(DEVCFG_BASE + SYSCTL_O_CPUSEL5) |= SYSCTL_CPUSEL5_SCI_A;  // 0 = CPU1, 1 = CPU2 : This register must be configured prior to enabling the peripheral clocks
    HWREG(CPUSYS_BASE + SYSCTL_O_PCLKCR7) |= SYSCTL_PCLKCR7_SCI_A;
    EDIS;

    //Allows CPU02 bootrom to take control of clock configuration registers
    EALLOW;
    HWREG(CLKCFG_BASE + SYSCTL_O_CLKSEM) = 0xA5A50000;
    HWREG(CLKCFG_BASE + SYSCTL_O_LOSPCP) = 0x0002;
    EDIS;

    // GPIO85=Rx, GPIO84=Tx to CPU2
    GPIO_setMasterCore(85, GPIO_CORE_CPU2);
    GPIO_setPinConfig(GPIO_85_SCIRXDA);
    GPIO_setDirectionMode(85, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(85, GPIO_PIN_TYPE_PULLUP);
    GPIO_setQualificationMode(85, GPIO_QUAL_ASYNC);

    GPIO_setMasterCore(84, GPIO_CORE_CPU2);
    GPIO_setPinConfig(GPIO_84_SCITXDA);
    GPIO_setDirectionMode(84, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(84, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(84, GPIO_QUAL_ASYNC);

#if 0   // no use from v11
    // Assign all GS RAMs to CPU2
    //MemCfg_setGSRAMMasterSel(MEMCFG_SECT_GSX_ALL, MEMCFG_GSRAMMASTER_CPU2);   // no use from v9.5
    EALLOW;
    HWREG(MEMCFG_BASE + MEMCFG_O_GSXMSEL) = 0xFFFF;    // NOTE : don't assign GSxRAM to CPU2 where .text of CPU1 is located !!!!!!!
    EDIS;
#endif

    HWREG(IPC_BASE + IPC_O_BOOTMODE)    = 1;            // CPU1 to CPU2 IPC Boot Mode Register = C1C2_BROM_BOOTMODE_BOOT_FROM_SCI
    HWREG(IPC_BASE + IPC_O_SENDCOM)     = 0x00000013;   // CPU1 to CPU2 IPC Command Register = BROM_IPC_EXECUTE_BOOTMODE_CMD
    HWREG(IPC_BASE + IPC_O_SET)         = 0x80000001;   // CPU1 to CPU2 IPC flag register

    //wait until CPU2 is booted and set IPC5
    while(!(HWREG(IPC_BASE + IPC_O_STS) & IPC_STS_IPC5));
    HWREG(IPC_BASE + IPC_O_ACK) |= IPC_ACK_IPC5;    //clearing the acknowledgement flag

#if 0   // no use from v11
    // Assign all GS RAMs to CPU1
    //MemCfg_setGSRAMMasterSel(MEMCFG_SECT_GSX_ALL, MEMCFG_GSRAMMASTER_CPU1);   // no use from v9.5
    EALLOW;
    HWREG(MEMCFG_BASE + MEMCFG_O_GSXMSEL) = 0x0000;
    EDIS;
#endif

    // SCIA connected to CPU1
    EALLOW;
    HWREG(CPUSYS_BASE + SYSCTL_O_PCLKCR7) &= ~SYSCTL_PCLKCR7_SCI_A;
    HWREG(DEVCFG_BASE + SYSCTL_O_CPUSEL5) &= ~SYSCTL_CPUSEL5_SCI_A;  // 0 = CPU1, 1 = CPU2 : This register must be configured prior to enabling the peripheral clocks
    HWREG(CPUSYS_BASE + SYSCTL_O_PCLKCR7) |= SYSCTL_PCLKCR7_SCI_A;
    EDIS;
}
#else // _FLASH

void easyDSP_FlashBootCPU2(void)
{
    // Loop until CPU02 control system IPC flags 1 and 32 are available
    while((HWREG(IPC_BASE + IPC_O_FLG) & IPC_FLAG0) || (HWREG(IPC_BASE + IPC_O_FLG) & IPC_FLAG31));

    HWREG(IPC_BASE + IPC_O_BOOTMODE)    = 0xB;          // CPU1 to CPU2 IPC Boot Mode Register = C1C2_BROM_BOOTMODE_BOOT_FROM_FLASH
    HWREG(IPC_BASE + IPC_O_SENDCOM)     = 0x00000013;   // CPU1 to CPU2 IPC Command Register = BROM_IPC_EXECUTE_BOOTMODE_CMD
    HWREG(IPC_BASE + IPC_O_SET)         = 0x80000001;   // CPU1 to CPU2 IPC flag register
}
#endif // _FLASH
#endif // CPU1

#ifdef CPU2
#ifndef _FLASH
#include "inc/hw_ipc.h"
void easyDSP_SCIBootCPU2(void)
{
    // booting echo feedback still ongoing even this moment
    // So no switch SCIA channel to CPU1 immediately
    DEVICE_DELAY_US(1000*30);
    DEVICE_DELAY_US(1000*30);
    DEVICE_DELAY_US(1000*30);
    DEVICE_DELAY_US(1000*10);

    // set IPC5 to inform to CPU1 that booting is done
    HWREG(IPC_BASE + IPC_O_SET) |= IPC_SET_IPC5;
    while(HWREG(IPC_BASE + IPC_O_FLG) & IPC_FLG_IPC5);
    asm(" NOP");
    asm(" NOP");

    // Wait until CPU1 is configuring SCI-B for CPU2 in easyDSP_SCI_Init()
    DEVICE_DELAY_US(1000*10);
}
#endif  // _FLASH
#endif  // CPU2
#endif  // F2837xD_CPU1_CPU2




/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// easyDSP communication
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////

// easyDSP commands & states
#define STAT_INIT       0
#define STAT_ADDR       1
#define STAT_DATA2B     2
#define STAT_DATA4B     3
#define STAT_WRITE      4
#define STAT_DATA8B     5
#define STAT_CONFIRM    6
#define STAT_DATA_BLOCK 7

#define CMD_ADDR            0xE7
#define CMD_ADDR_32BIT      0x41    // 32bit address from v10.1
//#define CMD_READ1B        0x12  not used
#define CMD_READ2B          0xDB
#define CMD_READ4B          0xC3
#define CMD_READ8B          0x8B
#define CMD_READ16B         0x28
//#define CMD_DATA1B        0x30  not used
#define CMD_DATA2B          0xBD
#define CMD_DATA4B          0x99
#define CMD_DATA8B          0x64
#define CMD_WRITE           0x7E
#define CMD_FB_READ         0x0D
#define CMD_FB_WRITE_OK     0x0D
#define CMD_FB_WRITE_NG     0x3C
#define CMD_DATA_BLOCK      0x55
#define CMD_CHANGE_CPU      0X5A
#define CMD_CONFIRM         0xA5

// error counter
unsigned int ezDSP_uBRKDTCount = 0, ezDSP_uFECount = 0, ezDSP_uOECount = 0, ezDSP_uPECount = 0;
unsigned int ezDSP_uWrongISRCount = 0;

// for easyDSP
unsigned char ezDSP_ucRx = 0;
unsigned int ezDSP_uState = STAT_INIT, ezDSP_uData = 0, ezDSP_uChksum = 0;
unsigned long ezDSP_ulData = 0, ezDSP_ulAddr = 0;
unsigned int ezDSP_uAddrRdCnt = 0, ezDSP_uDataRdCnt = 0;
unsigned long long ezDSP_ullData = 0;
unsigned int ezDSP_uBlockSize = 0, ezDSP_uBlockIndex = 0, ezDSP_uChkSumCalculated = 0;
unsigned int ezDSP_uCommand = 0;

unsigned int ezDSP_uISRRxCount = 0, ezDSP_uISRTxCount = 0;
unsigned int ezDSP_uRxFifoCnt = 0, ezDSP_uMaxRxFifoCnt = 0, ezDSP_uTxFifoCnt = 0, ezDSP_uMaxTxFifoCnt = 0;

unsigned int ezDSP_uTxFifoFullCnt = 0;      // something wrong if not zero
static inline void easy_addRing(char y) {
    if(SCI_getTxFIFOStatus(SCIx_BASE) != SCI_FIFO_TX16)     SCI_writeCharNonBlocking(SCIx_BASE, y);
    else                                                    ezDSP_uTxFifoFullCnt++;

    // counting after input to Tx
    ezDSP_uTxFifoCnt = SCI_getTxFIFOStatus(SCIx_BASE);
    if(ezDSP_uTxFifoCnt > ezDSP_uMaxTxFifoCnt) ezDSP_uMaxTxFifoCnt = ezDSP_uTxFifoCnt;
}

__interrupt void easy_RXINT_ISR(void)
{
    INT_NESTING_START;

    unsigned int uIndex;
    unsigned int u16X;

    ezDSP_uISRRxCount++;

    // check RX Error
    u16X = SCI_getRxStatus(SCIx_BASE);
    if(u16X & SCI_RXSTATUS_ERROR) {
        if(u16X & SCI_RXSTATUS_BREAK)   ezDSP_uBRKDTCount++;    // Break Down
        if(u16X & SCI_RXSTATUS_FRAMING) ezDSP_uFECount++;       // FE
        if(u16X & SCI_RXSTATUS_OVERRUN) ezDSP_uOECount++;       // OE
        if(u16X & SCI_RXSTATUS_PARITY)  ezDSP_uPECount++;       // PE

        // 'Break down' stops further Rx operation.
        // software reset is necessary to clear its status bit and proceed further rx operation
        SCI_performSoftwareReset(SCIx_BASE);

        SCI_clearOverflowStatus(SCIx_BASE);
        SCI_clearInterruptStatus(SCIx_BASE, SCI_INT_RXFF);
        INT_NESTING_END;
        return;
    }

    // monitoring
    ezDSP_uRxFifoCnt = SCI_getRxFIFOStatus(SCIx_BASE);
    if(ezDSP_uRxFifoCnt > ezDSP_uMaxRxFifoCnt) ezDSP_uMaxRxFifoCnt = ezDSP_uRxFifoCnt;

    if(SCI_getRxFIFOStatus(SCIx_BASE) == SCI_FIFO_RX0) {
        ezDSP_uWrongISRCount++;
        SCI_clearOverflowStatus(SCIx_BASE);
        SCI_clearInterruptStatus(SCIx_BASE, SCI_INT_RXFF);
        INT_NESTING_END;
        return;
    }

    // FIFO
    while(SCI_getRxFIFOStatus(SCIx_BASE) != SCI_FIFO_RX0) {
        ezDSP_ucRx = SCI_readCharNonBlocking(SCIx_BASE);

        // loop back for test
        //SCI_writeCharNonBlocking(SCIx_BASE, ezDSP_ucRx);
        //SCI_clearOverflowStatus(SCIx_BASE);
        //SCI_clearInterruptStatus(SCIx_BASE, SCI_INT_RXFF);
        //INT_NESTING_END;
        //return;

        ////////////////////////////////////////////
        // Parsing by state
        ////////////////////////////////////////////

        if(ezDSP_uState == STAT_INIT) {
            if(ezDSP_ucRx == CMD_ADDR || ezDSP_ucRx == CMD_ADDR_32BIT) {
                ezDSP_uState = STAT_ADDR;
                ezDSP_uAddrRdCnt = 0;
                ezDSP_b32bitAddress = (ezDSP_ucRx == CMD_ADDR_32BIT);
            }
            else if(ezDSP_ucRx == CMD_READ2B) {
                ezDSP_ulAddr++; // auto increment
                ezDSP_uData = *(unsigned int*)ezDSP_ulAddr;

                easy_addRing(ezDSP_uData >> 8);  // MSB
                easy_addRing(ezDSP_uData);       // LSB
                easy_addRing(CMD_FB_READ);

                ezDSP_uState = STAT_INIT;
            }
            else if(ezDSP_ucRx == CMD_READ16B) {
                ezDSP_ulAddr += 8;
                for(uIndex = 0; uIndex < 8; uIndex++) {
                    // Since this is not for variable, address is increased sequentially
                    ezDSP_uData = *(unsigned int*)(ezDSP_ulAddr + uIndex);
                    easy_addRing(ezDSP_uData >> 8);      // MSB
                    easy_addRing(ezDSP_uData);           // LSB
                }
                easy_addRing(CMD_FB_READ);

                ezDSP_uState = STAT_INIT;
            }
            else if(ezDSP_ucRx == CMD_DATA2B) {
                ezDSP_ulAddr++; // auto increment

                ezDSP_uState = STAT_DATA2B;
                ezDSP_uDataRdCnt = 0;
            }
            else if(ezDSP_ucRx == CMD_DATA4B) {
                ezDSP_ulAddr += 2;  // auto increment

                ezDSP_uState = STAT_DATA4B;
                ezDSP_uDataRdCnt = 0;
            }
            else if(ezDSP_ucRx == CMD_DATA8B) {
                ezDSP_ulAddr += 4;  // auto increment

                ezDSP_uState = STAT_DATA8B;
                ezDSP_uDataRdCnt = 0;
            }
        }
        else if(ezDSP_uState == STAT_ADDR) {
            ezDSP_uAddrRdCnt++;
            if(ezDSP_uAddrRdCnt == 1) {
                ezDSP_ulAddr = ezDSP_ucRx;          // MSB
            }
            else if(ezDSP_uAddrRdCnt == 2 || ezDSP_uAddrRdCnt == 3 || (ezDSP_b32bitAddress && ezDSP_uAddrRdCnt == 4 )) {
                ezDSP_ulAddr <<= 8;
                ezDSP_ulAddr |= ezDSP_ucRx;
            }
            else if((!ezDSP_b32bitAddress && ezDSP_uAddrRdCnt == 4) || (ezDSP_b32bitAddress && ezDSP_uAddrRdCnt == 5)) {
                if(ezDSP_ucRx == CMD_READ2B) {
                    ezDSP_uData = *(unsigned int*)ezDSP_ulAddr;

                    easy_addRing(ezDSP_uData >> 8);  // MSB
                    easy_addRing(ezDSP_uData);       // LSB
                    easy_addRing(CMD_FB_READ);

                    ezDSP_uState = STAT_INIT;
                }
                else if(ezDSP_ucRx == CMD_READ4B) {
                    ezDSP_ulData = *(unsigned long *)ezDSP_ulAddr;
                    easy_addRing(ezDSP_ulData >> 24);  // MSB
                    easy_addRing(ezDSP_ulData >> 16);
                    easy_addRing(ezDSP_ulData >> 8);
                    easy_addRing(ezDSP_ulData);        // LSB
                    easy_addRing(CMD_FB_READ);
                    ezDSP_uState = STAT_INIT;
                }
                else if(ezDSP_ucRx == CMD_READ8B) {
                    ezDSP_ullData = *(unsigned long long *)ezDSP_ulAddr;
                    easy_addRing(ezDSP_ullData >> (8*7));   // MSB
                    easy_addRing(ezDSP_ullData >> (8*6));
                    easy_addRing(ezDSP_ullData >> (8*5));
                    easy_addRing(ezDSP_ullData >> (8*4));
                    easy_addRing(ezDSP_ullData >> (8*3));
                    easy_addRing(ezDSP_ullData >> (8*2));
                    easy_addRing(ezDSP_ullData >> (8*1));
                    easy_addRing(ezDSP_ullData);            // LSB
                    easy_addRing(CMD_FB_READ);
                    ezDSP_uState = STAT_INIT;
                }
                else if(ezDSP_ucRx == CMD_READ16B) {
                    for(uIndex = 0; uIndex < 8; uIndex++) {
                        // Since this is not for variable, address is increased sequentially
                        ezDSP_uData = *(unsigned int*)(ezDSP_ulAddr + uIndex);
                        easy_addRing(ezDSP_uData >> 8);      // MSB
                        easy_addRing(ezDSP_uData);           // LSB
                    }
                    easy_addRing(CMD_FB_READ);
                    ezDSP_uState = STAT_INIT;
                }
                else if(ezDSP_ucRx == CMD_DATA2B) {
                    ezDSP_uState = STAT_DATA2B;
                    ezDSP_uDataRdCnt = 0;
                }
                else if(ezDSP_ucRx == CMD_DATA4B) {
                    ezDSP_uState = STAT_DATA4B;
                    ezDSP_uDataRdCnt = 0;
                }
                else if(ezDSP_ucRx == CMD_DATA8B) {
                    ezDSP_uState = STAT_DATA8B;
                    ezDSP_uDataRdCnt = 0;
                }
                else ezDSP_uState = STAT_INIT;
            }
            else
                ezDSP_uState = STAT_INIT;
        }
        else if(ezDSP_uState == STAT_DATA2B) {
            ezDSP_uDataRdCnt++;
            if(ezDSP_uDataRdCnt == 1)       ezDSP_uData = ezDSP_ucRx << 8;      // MSB
            else if(ezDSP_uDataRdCnt == 2)  ezDSP_uData |= ezDSP_ucRx;          // LSB
            else if(ezDSP_uDataRdCnt == 3)  ezDSP_uChksum = ezDSP_ucRx << 8;    // MSB
            else if(ezDSP_uDataRdCnt == 4)  ezDSP_uChksum |= ezDSP_ucRx;        // LSB
            else if(ezDSP_uDataRdCnt == 5) {
                if(ezDSP_ucRx == CMD_WRITE) {
                    if(ezDSP_uChksum == ((ezDSP_ulAddr + ezDSP_uData) & 0xFFFF)) {
                        *(unsigned int*)ezDSP_ulAddr = ezDSP_uData;
                        easy_addRing(CMD_FB_WRITE_OK);
                     }
                    else {
                        easy_addRing(CMD_FB_WRITE_NG);
                    }
                }
                ezDSP_uState = STAT_INIT;
            }
        }
        else if(ezDSP_uState == STAT_DATA4B) {
            ezDSP_uDataRdCnt++;
            if(ezDSP_uDataRdCnt == 1) {
                ezDSP_ulData = ezDSP_ucRx;      // MSB
                ezDSP_ulData <<= 8;
            }
            else if(ezDSP_uDataRdCnt == 2) {
                ezDSP_ulData |= ezDSP_ucRx;
                ezDSP_ulData <<= 8;
            }
            else if(ezDSP_uDataRdCnt == 3) {
                ezDSP_ulData |= ezDSP_ucRx;
                ezDSP_ulData <<= 8;
            }
            else if(ezDSP_uDataRdCnt == 4) {
                ezDSP_ulData |= ezDSP_ucRx;
            }
            else if(ezDSP_uDataRdCnt == 5)
                ezDSP_uChksum = ezDSP_ucRx << 8;    // MSB
            else if(ezDSP_uDataRdCnt == 6)
                ezDSP_uChksum |= ezDSP_ucRx;        // LSB
            else if(ezDSP_uDataRdCnt == 7) {
                if(ezDSP_ucRx == CMD_WRITE) {
                    if(ezDSP_uChksum == ((ezDSP_ulAddr + ezDSP_ulData) & 0xFFFF)) {
                        *(unsigned long*)ezDSP_ulAddr = ezDSP_ulData;
                        easy_addRing(CMD_FB_WRITE_OK);
                    }
                    else {
                        easy_addRing(CMD_FB_WRITE_NG);
                    }
                }
                ezDSP_uState = STAT_INIT;
            }
        }
        else if(ezDSP_uState == STAT_DATA8B) {
            ezDSP_uDataRdCnt++;
            if(ezDSP_uDataRdCnt == 1) {
                ezDSP_ullData = ezDSP_ucRx;         // MSB
                ezDSP_ullData <<= 8;
            }
            else if(ezDSP_uDataRdCnt == 2) {
                ezDSP_ullData |= ezDSP_ucRx;
                ezDSP_ullData <<= 8;
            }
            else if(ezDSP_uDataRdCnt == 3) {
                ezDSP_ullData |= ezDSP_ucRx;
                ezDSP_ullData <<= 8;
            }
            else if(ezDSP_uDataRdCnt == 4) {
                ezDSP_ullData |= ezDSP_ucRx;
                ezDSP_ullData <<= 8;
            }
            else if(ezDSP_uDataRdCnt == 5) {
                ezDSP_ullData |= ezDSP_ucRx;
                ezDSP_ullData <<= 8;
            }
            else if(ezDSP_uDataRdCnt == 6) {
                ezDSP_ullData |= ezDSP_ucRx;
                ezDSP_ullData <<= 8;
            }
            else if(ezDSP_uDataRdCnt == 7) {
                ezDSP_ullData |= ezDSP_ucRx;
                ezDSP_ullData <<= 8;
            }
            else if(ezDSP_uDataRdCnt == 8) {
                ezDSP_ullData |= ezDSP_ucRx;
            }
            else if(ezDSP_uDataRdCnt == 9)
                ezDSP_uChksum = ezDSP_ucRx << 8;    // MSB
            else if(ezDSP_uDataRdCnt == 10)
                ezDSP_uChksum |= ezDSP_ucRx;        // LSB
            else if(ezDSP_uDataRdCnt == 11) {
                if(ezDSP_ucRx == CMD_WRITE) {
                    if(ezDSP_uChksum == ((ezDSP_ulAddr + ezDSP_ullData) & 0xFFFF)) {
                        *(unsigned long long*)ezDSP_ulAddr = ezDSP_ullData;
                        easy_addRing(CMD_FB_WRITE_OK);
                    }
                    else {
                        easy_addRing(CMD_FB_WRITE_NG);
                    }
                }
                ezDSP_uState = STAT_INIT;
            }
        }

        else {
            ezDSP_uState = STAT_INIT;
        }
    }

    SCI_clearOverflowStatus(SCIx_BASE);
    SCI_clearInterruptStatus(SCIx_BASE, SCI_INT_RXFF);

    INT_NESTING_END;
}
