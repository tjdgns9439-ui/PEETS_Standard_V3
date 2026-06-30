//*****************************************************************************
//
// enet.c - Ethernet driver functionalities to support lwIP.
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


#include "enet.h"

#ifdef ETHERNET_DEBUG
DriverStats g_uiEnetStats = {0};
#endif /* ETHERNET_DEBUG */

void Ethernet_setMACConfigurationCustom(uint32_t base, uint32_t flags)
{
    HWREG(base + ETHERNET_O_MAC_CONFIGURATION) |= flags;
}
void Ethernet_clearMACConfigurationCustom(uint32_t base, uint32_t flags)
{
    HWREG(base + ETHERNET_O_MAC_CONFIGURATION) &= ~flags;
}

/***********************************************************************************************
 * Ethernet_genericISRCustom
 * 
 * This function is a custom interrupt service routine (ISR) for handling Ethernet
 * generic interrupts. It is called when a generic Ethernet interrupt occurs.
 * 
***********************************************************************************************/

interrupt void Ethernet_genericISRCustom(void)
{

    ENET_DRIVER_STATS_INC(GenericInterrupt);  

    Ethernet_RxChDesc *rxChan;
    Ethernet_TxChDesc *txChan;
    Ethernet_HW_descriptor    *descPtr;
    Ethernet_HW_descriptor    *tailPtr;
    uint16_t i=0;
    Ethernet_clearMACConfigurationCustom(Ethernet_device_struct.baseAddresses.enet_base,ETHERNET_MAC_CONFIGURATION_RE);
    Ethernet_clearMACConfigurationCustom(Ethernet_device_struct.baseAddresses.enet_base,ETHERNET_MAC_CONFIGURATION_TE);
    for(i = 0U;i < Ethernet_device_struct.initConfig.numChannels;i++)
     {
         Ethernet_disableRxDMAReception(
               Ethernet_device_struct.baseAddresses.enet_base,
               i);
     }
    if(((ETHERNET_DMA_CH0_STATUS_AIS |
                         ETHERNET_DMA_CH0_STATUS_RBU) ==
                       (HWREG(Ethernet_device_struct.baseAddresses.enet_base +
                              ETHERNET_O_DMA_CH0_STATUS) &
                              (uint32_t)(ETHERNET_DMA_CH0_STATUS_AIS |
                                         ETHERNET_DMA_CH0_STATUS_RBU))) ||
          (ETHERNET_MTL_Q0_INTERRUPT_CONTROL_STATUS_RXOVFIS) ==
                                   (HWREG(Ethernet_device_struct.baseAddresses.enet_base +
                                          ETHERNET_O_MTL_Q0_INTERRUPT_CONTROL_STATUS) &
                                          (uint32_t)(ETHERNET_MTL_Q0_INTERRUPT_CONTROL_STATUS_RXOVFIS
                                                     )))
      {
          if((ETHERNET_DMA_CH0_STATUS_AIS |
                             ETHERNET_DMA_CH0_STATUS_RBU) ==
                           (HWREG(Ethernet_device_struct.baseAddresses.enet_base +
                                  ETHERNET_O_DMA_CH0_STATUS) &
                                  (uint32_t)(ETHERNET_DMA_CH0_STATUS_AIS |
                                             ETHERNET_DMA_CH0_STATUS_RBU)))
          {
                ENET_DRIVER_STATS_INC(RBUinterrupt);
          }
          if((ETHERNET_MTL_Q0_INTERRUPT_CONTROL_STATUS_RXOVFIS) ==
                  (HWREG(Ethernet_device_struct.baseAddresses.enet_base +
                         ETHERNET_O_MTL_Q0_INTERRUPT_CONTROL_STATUS) &
                         (uint32_t)(ETHERNET_MTL_Q0_INTERRUPT_CONTROL_STATUS_RXOVFIS
                                    )))
          {
              ENET_DRIVER_STATS_INC(ROVinterrupt);
              Ethernet_enableMTLInterrupt(Ethernet_device_struct.baseAddresses.enet_base,0,
                                          ETHERNET_MTL_Q0_INTERRUPT_CONTROL_STATUS_RXOVFIS);
          }

            /*
             * Clear the AIS and RBU status bit. These MUST be
             * cleared together!
             */
            Ethernet_clearDMAChannelInterrupt(
                    Ethernet_device_struct.baseAddresses.enet_base,
                    ETHERNET_DMA_CHANNEL_NUM_0,
                    ETHERNET_DMA_CH0_STATUS_AIS |
                    ETHERNET_DMA_CH0_STATUS_RBU);

            Ethernet_disableDmaInterrupt(Ethernet_device_struct.baseAddresses.enet_base,
                                 0, (ETHERNET_DMA_CH0_STATUS_AIS |
                                     ETHERNET_DMA_CH0_STATUS_RBU));

    }
    if(0U != (HWREG(Ethernet_device_struct.baseAddresses.enet_base +
                                 ETHERNET_O_DMA_CH0_STATUS) &
                           (uint32_t) ETHERNET_DMA_CH0_STATUS_RI))
    {
        ENET_DRIVER_STATS_INC(RIinterrupt);

        Ethernet_clearDMAChannelInterrupt(
                        Ethernet_device_struct.baseAddresses.enet_base,
                        ETHERNET_DMA_CHANNEL_NUM_0,
                        ETHERNET_DMA_CH0_STATUS_NIS | ETHERNET_DMA_CH0_STATUS_RI);
    }

    for(i = 0U;i < Ethernet_device_struct.initConfig.numChannels;i++)
     {
         Ethernet_enableRxDMAReception(
               Ethernet_device_struct.baseAddresses.enet_base,
               i);
     }
    Ethernet_setMACConfigurationCustom(Ethernet_device_struct.baseAddresses.enet_base,ETHERNET_MAC_CONFIGURATION_RE);
    Ethernet_setMACConfigurationCustom(Ethernet_device_struct.baseAddresses.enet_base,ETHERNET_MAC_CONFIGURATION_TE);
}


