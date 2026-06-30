//###########################################################################
//
// FILE:   enet_lwip.c
//
// TITLE:  lwIP based Ethernet Example.
//
//###########################################################################
// $TI Release: $
// $Release Date: $
//
// C2000Ware v26.00.00.00
//
// Copyright (C) 2024 Texas Instruments Incorporated - http://www.ti.com
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
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
// $
//###########################################################################

#include <string.h>

#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_nvic.h"
#include "inc/hw_types.h"
#include "inc/hw_sysctl.h"
#include "inc/hw_emac.h"

#include "driverlib_cm/ethernet.h"
#include "driverlib_cm/gpio.h"
#include "driverlib_cm/interrupt.h"
#include "driverlib_cm/flash.h"
#include "driverlib_cm/uart.h"

#include "driverlib_cm/sysctl.h"
#include "driverlib_cm/systick.h"

#include "utils/lwiplib.h"
#include "driver/device_cm.h"
#include "driver/enet.h"
#include "board_drivers/pinout.h"

#include "lwip/apps/httpd.h"

#include "lwipopts.h"
//*****************************************************************************
//
//! \addtogroup master_example_list
//! <h1>Ethernet with lwIP (enet_lwip)</h1>
//!
//! This example application demonstrates the operation of the F2838x
//! microcontroller Ethernet controller using the lwIP TCP/IP Stack. Once
//! programmed, the device sits endlessly waiting for ICMP ping requests. It
//! has a static IP address. To ping the device, the sender has to be in the
//! same network. The stack also supports ARP.
//!
//! For additional details on lwIP, refer to the lwIP web page at:
//! http://savannah.nongnu.org/projects/lwip/
//
//*****************************************************************************

#define MAKE_IP_ADDRESS(a0,a1,a2,a3) (((a0<<24) & 0xFF000000) | ((a1<<16) & 0x00FF0000) | ((a2<<8)  & 0x0000FF00) | (a3 & 0x000000FF) )

#define RX_ISR_BIT  1
#define TX_ISR_BIT  4

#define RX_ISR_MASK (1 << RX_ISR_BIT)
#define TX_ISR_MASK (1 << TX_ISR_BIT)

#define LWIP_PHY_REG_BMSR 1U
#define LWIP_PHY_REG_ID1  2U
#define LWIP_PHY_REG_ID2  3U
#define LWIP_PHY_BMSR_LINKSTAT 0x0004U
#define LWIP_PHY_ADDR 1U
#define LWIP_PING_LOG_IDLE_TIMEOUT_MS 60000U
#define LWIP_PING_LOG_UART_BAUDRATE 115200U

/*
g_uiISRsignal variable signals the Interrupt.
*/
uint32_t g_uiISRsignal = 0;
volatile uint32_t g_lwip_rx_callback_count = 0;
volatile uint32_t g_lwip_tx_release_count = 0;
volatile uint32_t g_lwip_rx_isr_count = 0;
volatile uint32_t g_lwip_tx_isr_count = 0;
volatile uint32_t g_lwip_rx_queue_drain_count = 0;
volatile uint32_t g_lwip_tx_queue_drain_count = 0;
volatile uint32_t g_lwip_rx_poll_drain_count = 0;
volatile uint32_t g_lwip_rx_recover_count = 0;
volatile uint32_t g_lwip_rx_dma_status_before_drain = 0;
volatile uint32_t g_lwip_rx_dma_status_after_drain = 0;
volatile uint32_t g_lwip_rx_service_stage = 0;
volatile uint32_t g_lwip_tx_service_stage = 0;
volatile uint32_t g_lwip_tx_dma_status_before_drain = 0;
volatile uint32_t g_lwip_tx_dma_status_after_drain = 0;
volatile uint32_t g_lwip_main_loop_count = 0;
volatile uint32_t g_lwip_init_stage = 0;
volatile uint32_t g_lwip_ethernet_init_step = 0;
volatile uint32_t g_lwip_mac_packet_filter = 0;
volatile uint32_t g_lwip_config_ip = 0;
volatile uint32_t g_lwip_config_mac_low = 0;
volatile uint32_t g_lwip_config_mac_high = 0;
volatile uint32_t g_lwip_last_eth_type = 0;
volatile uint32_t g_lwip_last_arp_target_ip = 0;
volatile uint32_t g_lwip_last_ipv4_dst_ip = 0;
volatile uint32_t g_lwip_arp_for_us_count = 0;
volatile uint32_t g_lwip_ipv4_for_us_count = 0;
volatile uint32_t g_lwip_phy_bmsr_live = 0;
volatile uint32_t g_lwip_phy_id1_live = 0;
volatile uint32_t g_lwip_phy_id2_live = 0;
volatile uint32_t g_lwip_phy_link_up_live = 0;
volatile uint32_t g_lwip_mac_config_live = 0;
volatile uint32_t g_lwip_dma_ch0_status_live = 0;
volatile uint32_t g_lwip_dma_ch0_rx_control_live = 0;
volatile uint32_t g_lwip_dma_ch0_tx_control_live = 0;
volatile uint32_t g_lwip_ms_ticks = 0;
volatile uint32_t g_lwip_1ms_service_count = 0;

extern volatile uint32_t g_f2838xif_ping_attempt_count;
extern volatile uint32_t g_f2838xif_ping_success_count;
extern volatile uint32_t g_f2838xif_ping_failure_count;
extern volatile uint32_t g_f2838xif_ping_success_rate_x100;
extern volatile uint32_t g_f2838xif_ping_failure_rate_x100;

static void ping_log_uart_write_string(const char *text)
{
    while(*text != '\0')
    {
        UART_writeChar(UART0_BASE, (uint8_t)*text);
        ++text;
    }
}

static void ping_log_uart_write_u32(uint32_t value)
{
    char digits[10];
    uint32_t count = 0U;

    if(value == 0U)
    {
        UART_writeChar(UART0_BASE, (uint8_t)'0');
        return;
    }

    while(value != 0U)
    {
        digits[count] = (char)('0' + (value % 10U));
        value /= 10U;
        ++count;
    }

    while(count != 0U)
    {
        --count;
        UART_writeChar(UART0_BASE, (uint8_t)digits[count]);
    }
}

static void ping_log_uart_write_rate(uint32_t rateX100)
{
    ping_log_uart_write_u32(rateX100 / 100U);
    UART_writeChar(UART0_BASE, (uint8_t)'.');
    if((rateX100 % 100U) < 10U)
    {
        UART_writeChar(UART0_BASE, (uint8_t)'0');
    }
    ping_log_uart_write_u32(rateX100 % 100U);
    UART_writeChar(UART0_BASE, (uint8_t)'%');
}

static void ping_log_uart_init(void)
{
    UART_setConfig(UART0_BASE,
                   UART_CLK_FREQ,
                   LWIP_PING_LOG_UART_BAUDRATE,
                   UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                       UART_CONFIG_PAR_NONE);
    HWREG(UART0_BASE + UART_O_CTL) &= ~UART_CTL_RXE;
}

static void ping_log_uart_write_startup(void)
{
    ping_log_uart_write_string("\r\nPING_LOG_UART_READY\r\n");
    ping_log_uart_write_string("idle_timeout_ms=");
    ping_log_uart_write_u32(LWIP_PING_LOG_IDLE_TIMEOUT_MS);
    ping_log_uart_write_string("\r\n");
}

static void ping_log_uart_write_summary(void)
{
    ping_log_uart_write_string("\r\nPING_TEST_SUMMARY\r\n");
    ping_log_uart_write_string("total_attempts=");
    ping_log_uart_write_u32(g_f2838xif_ping_attempt_count);
    ping_log_uart_write_string("\r\nsuccess_count=");
    ping_log_uart_write_u32(g_f2838xif_ping_success_count);
    ping_log_uart_write_string("\r\nfailure_count=");
    ping_log_uart_write_u32(g_f2838xif_ping_failure_count);
    ping_log_uart_write_string("\r\nsuccess_rate=");
    ping_log_uart_write_rate(g_f2838xif_ping_success_rate_x100);
    ping_log_uart_write_string("\r\nfailure_rate=");
    ping_log_uart_write_rate(g_f2838xif_ping_failure_rate_x100);
    ping_log_uart_write_string("\r\nEND_PING_TEST_SUMMARY\r\n");
}

static void ping_log_service(uint32_t nowMs)
{
    static uint32_t lastAttemptCount = 0U;
    static uint32_t lastActivityMs = 0U;
    static uint32_t summaryWritten = 0U;
    uint32_t currentAttemptCount = g_f2838xif_ping_attempt_count;

    if(currentAttemptCount != lastAttemptCount)
    {
        lastAttemptCount = currentAttemptCount;
        lastActivityMs = nowMs;
        summaryWritten = 0U;
        return;
    }

    if((currentAttemptCount != 0U) && (summaryWritten == 0U) &&
       ((nowMs - lastActivityMs) >= LWIP_PING_LOG_IDLE_TIMEOUT_MS))
    {
        ping_log_uart_write_summary();
        summaryWritten = 1U;
    }
}

extern void f2838xif_serviceTxQueue(void);
extern void f2838xif_noteTxComplete(void);
extern volatile uint32_t g_cm_to_cpu1_mailbox;

static void service_rx_queue_from_isr(void)
{
    ++g_lwip_rx_queue_drain_count;
    g_lwip_rx_service_stage = 1U;
    g_lwip_rx_dma_status_before_drain =
        HWREG(EMAC_BASE + ETHERNET_O_DMA_CH0_STATUS);

    g_lwip_rx_service_stage = 2U;
    Ethernet_removePacketsFromRxQueue(
        (Ethernet_DescCh*)&Ethernet_device_struct.dmaObj.rxDma[ETHERNET_DMA_CHANNEL_NUM_0],
        ETHERNET_COMPLETION_NORMAL);

    g_lwip_rx_service_stage = 3U;
    g_lwip_rx_dma_status_after_drain =
        HWREG(EMAC_BASE + ETHERNET_O_DMA_CH0_STATUS);
    g_lwip_rx_service_stage = 4U;
}

static void service_tx_queue_from_isr(void)
{
    ++g_lwip_tx_queue_drain_count;
    g_lwip_tx_service_stage = 1U;
    g_lwip_tx_dma_status_before_drain =
        HWREG(EMAC_BASE + ETHERNET_O_DMA_CH0_STATUS);

    g_lwip_tx_service_stage = 2U;
    Ethernet_removePacketsFromTxQueue(
        (Ethernet_DescCh*)&Ethernet_device_struct.dmaObj.txDma[ETHERNET_DMA_CHANNEL_NUM_0],
        ETHERNET_COMPLETION_NORMAL);

    g_lwip_tx_service_stage = 3U;
    f2838xif_noteTxComplete();
    g_lwip_tx_dma_status_after_drain =
        HWREG(EMAC_BASE + ETHERNET_O_DMA_CH0_STATUS);
    g_lwip_tx_service_stage = 4U;
}

static void capture_ethernet_live_status(void)
{
    uint32_t bmsr;

    Ethernet_configurePHYAddress(EMAC_BASE, LWIP_PHY_ADDR);
    bmsr = Ethernet_readPHYRegister(EMAC_BASE, LWIP_PHY_REG_BMSR);
    g_lwip_phy_bmsr_live = bmsr;
    g_lwip_phy_id1_live = Ethernet_readPHYRegister(EMAC_BASE, LWIP_PHY_REG_ID1);
    g_lwip_phy_id2_live = Ethernet_readPHYRegister(EMAC_BASE, LWIP_PHY_REG_ID2);
    g_lwip_phy_link_up_live =
        ((bmsr & LWIP_PHY_BMSR_LINKSTAT) != 0U) ? 1U : 0U;
    g_lwip_mac_config_live =
        HWREG(EMAC_BASE + ETHERNET_O_MAC_CONFIGURATION);
    g_lwip_dma_ch0_status_live =
        HWREG(EMAC_BASE + ETHERNET_O_DMA_CH0_STATUS);
    g_lwip_dma_ch0_rx_control_live =
        HWREG(EMAC_BASE + ETHERNET_O_DMA_CH0_RX_CONTROL);
    g_lwip_dma_ch0_tx_control_live =
        HWREG(EMAC_BASE + ETHERNET_O_DMA_CH0_TX_CONTROL);
}

static uint32_t read_be32(const uint8_t *p)
{
    return (((uint32_t)p[0]) << 24) |
           (((uint32_t)p[1]) << 16) |
           (((uint32_t)p[2]) << 8) |
           ((uint32_t)p[3]);
}

static void capture_rx_packet_info(Ethernet_Pkt_Desc *pPacket)
{
    uint8_t *data;
    uint32_t ethType;
    uint32_t ip;

    if ((pPacket == 0) || (pPacket->dataBuffer == 0) ||
        (pPacket->pktLength < 14U))
    {
        return;
    }

    data = pPacket->dataBuffer;
    ethType = (((uint32_t)data[12]) << 8) | ((uint32_t)data[13]);
    g_lwip_last_eth_type = ethType;

    if ((ethType == 0x0806U) && (pPacket->pktLength >= 42U))
    {
        ip = read_be32(&data[38]);
        g_lwip_last_arp_target_ip = ip;
        if (ip == g_lwip_config_ip)
        {
            ++g_lwip_arp_for_us_count;
        }
    }
    else if ((ethType == 0x0800U) && (pPacket->pktLength >= 34U))
    {
        ip = read_be32(&data[30]);
        g_lwip_last_ipv4_dst_ip = ip;
        if (ip == g_lwip_config_ip)
        {
            ++g_lwip_ipv4_for_us_count;
        }
    }
}


const unsigned long IPAddr =  0x6F6F6F65; // 111.111.111.101
const unsigned long NetMask = 0xFFFFFF00;
const unsigned long GWAddr = 0x00000000;

Ethernet_Handle emac_handle;
Ethernet_InitConfig *pInitCfg;
extern Ethernet_Device Ethernet_device_struct;

/*
systick Timer acts as source for lwip timer,
125000 implies 1ms.
*/
const uint32_t systickPeriodValue = 125000;


/***********************************************************************************************
 * Ethernet_transmitISRCustom
 *
 * This function is a custom interrupt service routine (ISR) for handling Ethernet
 * transmission interrupts. It is called when a packet is succesfully transmitted.
 *
 ************************************************************************************************/
void Ethernet_transmitISRCustom(void);
/************************************************************************************************
 * Ethernet_receiveISRCustom
 *
 * This function is a custom interrupt service routine (ISR) for handling Ethernet
 * reception interrupts. It is called when a packet is received.
 *
************************************************************************************************/
void Ethernet_receiveISRCustom(void);
//*****************************************************************************
//
//  This function is a callback function called by the example to
//  get a Packet Buffer. Has to return a Ethernet_Pkt_Desc Structure.
//  Rewrite this API for custom use case.
//
//*****************************************************************************
Ethernet_Pkt_Desc* Ethernet_getPacketBufferCustom(void)
{
    Ethernet_Pkt_Desc* pktPtr = lwIP_getFreePacket();
    ENET_DRIVER_STATS_INC(RXgetPacketBuffer);
    return (pktPtr);
}

//*****************************************************************************
//
//  This is a hook function and called by the driver when it receives a
//  packet. Application is expected to replenish the buffer after consuming it.
//  Has to return a ETHERNET_Pkt_Desc Structure.
//  Rewrite this API for custom use case.
//
//*****************************************************************************
Ethernet_Pkt_Desc* Ethernet_receivePacketCallbackCustom(
        Ethernet_Handle handleApplication,
        Ethernet_Pkt_Desc *pPacket)
{

	Ethernet_Pkt_Desc* temp_eth_pkt;

    ENET_DRIVER_STATS_INC(RXPacketCallback);
    ++g_lwip_rx_callback_count;
    capture_rx_packet_info(pPacket);

      temp_eth_pkt=lwIPEthernetIntHandler(pPacket);

      return temp_eth_pkt;
}

void Ethernet_releaseTxPacketBufferCustom(
        Ethernet_Handle handleApplication,
        Ethernet_Pkt_Desc *pPacket)
{
    //
    // Once the packet is sent, reuse the packet memory to avoid
    // memory leaks. Call this interrupt handler function which will take care
    // of freeing the memory used by the packet descriptor.
    //
    lwIPEthernetIntHandler(pPacket);
    ++g_lwip_tx_release_count;


    ENET_DRIVER_STATS_INC(TXreleasePacket);
}

void
Ethernet_init(const unsigned char *mac)
{

    Ethernet_InitInterfaceConfig initInterfaceConfig;
    uint32_t macLower;
    uint32_t macHigher;
    uint8_t *temp;

    g_lwip_ethernet_init_step = 1U;
    initInterfaceConfig.ssbase = EMAC_SS_BASE;
    initInterfaceConfig.enet_base = EMAC_BASE;
    initInterfaceConfig.phyMode = ETHERNET_SS_PHY_INTF_SEL_MII;
    g_lwip_ethernet_init_step = 2U;

    //
    // Assign SoC specific functions for Enabling,Disabling interrupts
    // and for enabling the Peripheral at system level
    //
    initInterfaceConfig.ptrPlatformInterruptDisable =
                                                    &Platform_disableInterrupt;
    initInterfaceConfig.ptrPlatformInterruptEnable =
                                                     &Platform_enableInterrupt;
    initInterfaceConfig.ptrPlatformPeripheralEnable =
                                                    &Platform_enablePeripheral;
    initInterfaceConfig.ptrPlatformPeripheralReset =
                                                     &Platform_resetPeripheral;
    initInterfaceConfig.ptrCoreInterruptDisable =
                                                     &Interrupt_disableInProcessor;
    initInterfaceConfig.ptrCoreInterruptEnable =
                                                     &Interrupt_enableInProcessor;

    //
    // Assign the peripheral number at the SoC
    //
    initInterfaceConfig.peripheralNum = SYSCTL_PERIPH_CLK_ENET;

    //
    // Assign the default SoC specific interrupt numbers of Ethernet interrupts
    //
    initInterfaceConfig.interruptNum[0] = INT_EMAC;
    initInterfaceConfig.interruptNum[1] = INT_EMAC_TX0;
    initInterfaceConfig.interruptNum[2] = INT_EMAC_TX1;
    initInterfaceConfig.interruptNum[3] = INT_EMAC_RX0;
    initInterfaceConfig.interruptNum[4] = INT_EMAC_RX1;
    g_lwip_ethernet_init_step = 3U;

    pInitCfg = Ethernet_initInterface(initInterfaceConfig);
    g_lwip_ethernet_init_step = 4U;

    Ethernet_getInitConfig(pInitCfg);
    pInitCfg->dmaMode.InterruptMode = ETHERNET_DMA_MODE_INTM_MODE2;
    g_lwip_ethernet_init_step = 5U;

    //
    // Assign the callbacks for Getting packet buffer when needed
    // Releasing the TxPacketBuffer on Transmit interrupt callbacks
    // Receive packet callback on Receive packet completion interrupt
    //
    pInitCfg->pfcbRxPacket = &Ethernet_receivePacketCallbackCustom;
    pInitCfg->pfcbGetPacket = &Ethernet_getPacketBufferCustom;
    pInitCfg->pfcbFreePacket = &Ethernet_releaseTxPacketBufferCustom;

    pInitCfg->numChannels = 1U;

    //
    // The Application handle is not used by this application
    // Hence using a dummy value of 1
    //
    g_lwip_ethernet_init_step = 6U;
    Ethernet_getHandle((Ethernet_Handle)1, pInitCfg , &emac_handle);
    Ethernet_configureMDIO(EMAC_BASE, 0U, 5U, 0U);
    Ethernet_configurePHYAddress(EMAC_BASE, LWIP_PHY_ADDR);
    g_lwip_ethernet_init_step = 7U;

    //
    // Disable transmit buffer unavailable and normal interrupt which
    // are enabled by default in Ethernet_getHandle.
    //
    Ethernet_disableDmaInterrupt(Ethernet_device_struct.baseAddresses.enet_base,
                                 0, (ETHERNET_DMA_CH0_INTERRUPT_ENABLE_TBUE |
                                     ETHERNET_DMA_CH0_INTERRUPT_ENABLE_NIE));
    g_lwip_ethernet_init_step = 8U;

    //
    // Enable the MTL interrupt to service the receive FIFO overflow
    // condition in the Ethernet module.
    //
    Ethernet_enableMTLInterrupt(Ethernet_device_struct.baseAddresses.enet_base,0,
                                ETHERNET_MTL_Q0_INTERRUPT_CONTROL_STATUS_RXOIE);
    g_lwip_ethernet_init_step = 9U;

    //
    // Disable the MAC Management counter interrupts as they are not used
    // in this application.
    //
    HWREG(Ethernet_device_struct.baseAddresses.enet_base + ETHERNET_O_MMC_RX_INTERRUPT_MASK) = 0xFFFFFFFF;
    HWREG(Ethernet_device_struct.baseAddresses.enet_base + ETHERNET_O_MMC_IPC_RX_INTERRUPT_MASK) = 0xFFFFFFFF;
	HWREG(Ethernet_device_struct.baseAddresses.enet_base + ETHERNET_O_MMC_TX_INTERRUPT_MASK) = 0xFFFFFFFF;
    //
    //Do global Interrupt Enable
    //
    (void)Interrupt_enableInProcessor();
    g_lwip_ethernet_init_step = 10U;

    //
    //Assign default ISRs
    //
    Interrupt_registerHandler(INT_EMAC_TX0, Ethernet_transmitISRCustom);
    Interrupt_registerHandler(INT_EMAC_RX0, Ethernet_receiveISRCustom);
    Interrupt_registerHandler(INT_EMAC, Ethernet_genericISRCustom);
    g_lwip_ethernet_init_step = 11U;


    //
    // Convert the mac address string into the 32/16 split variables format
    // that is required by the driver to program into hardware registers.
    // Note: This step is done after the Ethernet_getHandle function because
    //       a dummy MAC address is programmed in that function.
    //
    temp = (uint8_t *)&macLower;
    temp[0] = mac[0];
    temp[1] = mac[1];
    temp[2] = mac[2];
    temp[3] = mac[3];

    temp = (uint8_t *)&macHigher;
    temp[0] = mac[4];
    temp[1] = mac[5];

    //
    // Program the unicast mac address.
    //
    Ethernet_setMACAddr(EMAC_BASE,
                        0,
                        macHigher,
                        macLower,
                        ETHERNET_CHANNEL_0);
    g_lwip_ethernet_init_step = 12U;
    Ethernet_setMACPacketFilter(EMAC_BASE,
                                ETHERNET_MAC_PACKET_FILTER_PR |
                                ETHERNET_MAC_PACKET_FILTER_RA);
    g_lwip_mac_packet_filter =
        HWREG(EMAC_BASE + ETHERNET_O_MAC_PACKET_FILTER);
    g_lwip_ethernet_init_step = 13U;

    Ethernet_clearMACConfigurationCustom(Ethernet_device_struct.baseAddresses.enet_base,ETHERNET_MAC_CONFIGURATION_RE);
    Ethernet_setMACConfigurationCustom(Ethernet_device_struct.baseAddresses.enet_base,ETHERNET_MAC_CONFIGURATION_RE);
    g_lwip_ethernet_init_step = 14U;

    (&Ethernet_device_struct.dmaObj.txDma[ETHERNET_DMA_CHANNEL_NUM_0])->descCount = 0;
    g_lwip_ethernet_init_step = 15U;

}
void httpLEDToggle(void);
void(*ledtoggleFuncPtr)(void) = &httpLEDToggle;

//*****************************************************************************
//
// The interrupt handler for the SysTick interrupt.
//
//*****************************************************************************
void
SysTickIntHandler(void)
{
    //
    // Call the lwIP timer handler.
    //
    lwIPTimer(1);
    ++g_lwip_ms_ticks;
}

//*****************************************************************************
//
// This example demonstrates the use of the Ethernet Controller.
//
//*****************************************************************************
int
main(void)
{
    unsigned long ulUser0, ulUser1;
    unsigned char pucMACArray[8];

    //
    // User specific IP Address Configuration.
    // Current implementation works with Static IP address only.
    //
    unsigned long IPAddr = 0x6F6F6F65;
    unsigned long NetMask = 0xFFFFFF00;
    unsigned long GWAddr = 0x00000000;

    //
    // Initializing the CM. Loading the required functions to SRAM.
    //
    g_lwip_init_stage = 1U;
    Custom_CM_init();
    ping_log_uart_init();
    ping_log_uart_write_startup();
    g_lwip_init_stage = 2U;

    SYSTICK_setPeriod(systickPeriodValue);
    SYSTICK_enableCounter();
    SYSTICK_registerInterruptHandler(SysTickIntHandler);
    SYSTICK_enableInterrupt();
    g_lwip_init_stage = 3U;

    //
    // Enable processor interrupts.
    //
    Interrupt_enableInProcessor();

    // Set user/company specific MAC octets
    // (for this code we are using A8-63-F2-00-00-80)
    // 0x00 MACOCT3 MACOCT2 MACOCT1
    ulUser0 = 0x00F263A8;

    // 0x00 MACOCT6 MACOCT5 MACOCT4
    ulUser1 = 0x00800000;

    //
    // Convert the 24/24 split MAC address from NV ram into a 32/16 split MAC
    // address needed to program the hardware registers, then program the MAC
    // address into the Ethernet Controller registers.
    //
    pucMACArray[0] = ((ulUser0 >>  0) & 0xff);
    pucMACArray[1] = ((ulUser0 >>  8) & 0xff);
    pucMACArray[2] = ((ulUser0 >> 16) & 0xff);
    pucMACArray[3] = ((ulUser1 >>  0) & 0xff);
    pucMACArray[4] = ((ulUser1 >>  8) & 0xff);
    pucMACArray[5] = ((ulUser1 >> 16) & 0xff);
    g_lwip_config_ip = IPAddr;
    g_lwip_config_mac_low = ulUser0;
    g_lwip_config_mac_high = ulUser1;

    Interrupt_enable(INT_EMAC_TX0);
    Interrupt_enable(INT_EMAC_RX0);
    Interrupt_enable(INT_EMAC);

    //
    // Initialze the lwIP library, using DHCP.
    //
    lwIPInit(0, pucMACArray, IPAddr, NetMask, GWAddr, IPADDR_USE_STATIC);
    g_lwip_init_stage = 4U;

    //
    // Initialize ethernet module.
    //
    Ethernet_init(pucMACArray);
    capture_ethernet_live_status();
    g_lwip_init_stage = 5U;

    //
    // Initialize the netif and start lwIP
    //
    lwIPStart(0);
    capture_ethernet_live_status();
    g_lwip_init_stage = 6U;

    //
    // Initialize the HTTP webserver daemon.
    //
    httpd_init();
    g_lwip_init_stage = 7U;

    //
    // Loop forever. All the work is done in interrupt handlers.
    //
    while(1)
    {
        uint32_t uiISRsignal = g_uiISRsignal;
        static uint32_t lastMsTick = 0U;
        ++g_lwip_main_loop_count;
        g_cm_to_cpu1_mailbox = g_lwip_main_loop_count;
        if((g_lwip_main_loop_count & 0xFFFFU) == 0U)
        {
            capture_ethernet_live_status();
        }

        if(lastMsTick != g_lwip_ms_ticks)
        {
            lastMsTick = g_lwip_ms_ticks;
            ++g_lwip_1ms_service_count;
            ping_log_service(lastMsTick);
        }

        f2838xif_serviceTxQueue();

        if((uiISRsignal & RX_ISR_MASK) != 0)
        {
            ETHERNET_DISABLE_INTERRUPTS();
            g_uiISRsignal &= (~RX_ISR_MASK);
            ETHERNET_ENABLE_INTERRUPTS();
            service_rx_queue_from_isr();
        }

        if((uiISRsignal & TX_ISR_MASK) != 0)
        {
            ETHERNET_DISABLE_INTERRUPTS();
            g_uiISRsignal &= (~TX_ISR_MASK);
            ETHERNET_ENABLE_INTERRUPTS();
            service_tx_queue_from_isr();
        }
    }
}

void Ethernet_transmitISRCustom(void)
{

    ENET_DRIVER_STATS_INC(TXinterrupt);
    ++g_lwip_tx_isr_count;

    Ethernet_clearDMAChannelInterrupt(
            Ethernet_device_struct.baseAddresses.enet_base,
            ETHERNET_DMA_CHANNEL_NUM_0,
            ETHERNET_DMA_CH0_STATUS_TI);

    g_uiISRsignal |= TX_ISR_MASK;
}

void Ethernet_receiveISRCustom(void)
{

    ENET_DRIVER_STATS_INC(RXinterrupt);
    ++g_lwip_rx_isr_count;

    Ethernet_clearDMAChannelInterrupt(
            Ethernet_device_struct.baseAddresses.enet_base,
            ETHERNET_DMA_CHANNEL_NUM_0,
            ETHERNET_DMA_CH0_STATUS_RI);

    g_uiISRsignal |= RX_ISR_MASK;
}


//*****************************************************************************
//
// Called by lwIP Library. Toggles the led when a command is received by the
// HTTP webserver.
//
//*****************************************************************************
void httpLEDToggle(void)
{
    //
    // Toggle the LED D1 on the control card.
    //
    GPIO_togglePin(DEVICE_GPIO_PIN_LED1);
}


//*****************************************************************************
//
// Called by lwIP Library. Could be used for periodic custom tasks.
//
//*****************************************************************************
void lwIPHostTimerHandler(void)
{

}
