//###########################################################################
//
// FILE:   f2838xif.c
//
// TITLE:  F2838x interface port file.
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

/**
 * Copyright (c) 2001-2004 Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 *
 * Author: Adam Dunkels <adam@sics.se>
 *
 */

/**
 * Copyright (c) 2018 Texas Instruments Incorporated
 *
 * This file is dervied from the ``ethernetif.c'' skeleton Ethernet network
 * interface driver for lwIP.
 *
 */

#include <string.h>
/**
 * lwIP specific header files
 */
#include "lwip/opt.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"
#include <lwip/stats.h>
#include <lwip/snmp.h>
#include "netif/etharp.h"
#include "netif/ppp/pppoe.h"
#include "netif/f2838xif.h"

/**
 * f2838x device specific header files
 */
#include "inc/hw_emac.h"
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib_cm/ethernet.h"
#include "driverlib_cm/interrupt.h"
#include "driverlib_cm/sysctl.h"

#include "utils/lwiplib.h"
/**
 * Sanity Check:  This interface driver will NOT work if the following defines
 * are incorrect.
 *
 */
#if (PBUF_LINK_HLEN != 16)
#error "PBUF_LINK_HLEN must be 16 for this interface driver!"
#endif
#if (ETH_PAD_SIZE != 0)
#error "ETH_PAD_SIZE must be 0 for this interface driver!"
#endif
#if (!SYS_LIGHTWEIGHT_PROT)
#error "SYS_LIGHTWEIGHT_PROT must be enabled for this interface driver!"
#endif


#define EMAC_BASE                   0x400C0000U //EMAC
#define EMAC_SS_BASE                0x400C2000U //EMACSS

/*
 * Temporary Ethernet bring-up responder.
 * 1: answer TCP/80 SYN + HTTP GET with a tiny raw HTTP/1.0 200 response.
 * 0: let TCP traffic pass to lwIP.
 */
#ifndef CM_ETH_BRINGUP_HTTP
#define CM_ETH_BRINGUP_HTTP 1
#endif

/* Define those to better describe your network interface. */
#define IFNAME0 't'
#define IFNAME1 'i'

#define F2838X_NUM_PBUF_QUEUE   20

/* Helper struct to hold a queue of pbufs for transmit and receive. */
struct pbufq
{
    struct pbuf *pbuf[F2838X_NUM_PBUF_QUEUE];
    unsigned long qwrite;
    unsigned long qread;
    unsigned long overflow;
};

/* Helper macros for accessing pbuf queues. */
#define PBUF_QUEUE_EMPTY(q) \
    (((q)->qwrite == (q)->qread) ? true : false)

#define PBUF_QUEUE_FULL(q) \
    ((((((q)->qwrite + 1) % F2838X_NUM_PBUF_QUEUE)) == (q)->qread) ? \
    true : false )

/**
 * Helper struct to hold private data used to operate your ethernet interface.
 * Keeping the ethernet address of the MAC in this struct is not necessary
 * as it is already kept in the struct netif.
 * But this is only an example, anyway...
 */
struct f2838xif
{
    struct eth_addr *ethaddr;
    /* Add whatever per-interface state that is needed here. */
    struct pbufq txq;
    Ethernet_Pkt_Desc *pktDesc;
};

/**
 * A structure used to keep track of driver state and error counts.
 */
typedef struct {
    uint32_t ui32TXCount;
    uint32_t ui32TXCopyCount;
    uint32_t ui32TXCopyFailCount;
    uint32_t ui32TXNoDescCount;
    uint32_t ui32TXBufQueuedCount;
    uint32_t ui32TXBufFreedCount;
    uint32_t ui32RXBufReadCount;
    uint32_t ui32RXPacketReadCount;
    uint32_t ui32RXPacketFreedCount;
    uint32_t ui32RXPacketErrCount;
    uint32_t ui32RXPbufAllocFailCount;
    uint32_t ui32RXPacketPostFailedCount;
}
tDriverStats;

tDriverStats g_sDriverStats = {0};

volatile uint32_t g_f2838xif_input_ok_count = 0;
volatile uint32_t g_f2838xif_input_error_count = 0;
volatile uint32_t g_f2838xif_last_input_error = 0;
volatile uint32_t g_f2838xif_linkoutput_count = 0;
volatile uint32_t g_f2838xif_transmit_count = 0;
volatile uint32_t g_f2838xif_transmit_ok_count = 0;
volatile uint32_t g_f2838xif_transmit_fail_count = 0;
static volatile uint32_t g_f2838xif_rx_pbuf_eth_type = 0;
static volatile uint32_t g_f2838xif_rx_pbuf_arp_target_ip = 0;
static volatile uint32_t g_f2838xif_rx_pbuf_ipv4_dst_ip = 0;
static volatile uint32_t g_f2838xif_rx_pbuf_ipv4_proto = 0;
static volatile uint32_t g_f2838xif_rx_pbuf_icmp_type = 0;
static volatile uint32_t g_f2838xif_pbuf_arp_for_us_count = 0;
static volatile uint32_t g_f2838xif_pbuf_ipv4_for_us_count = 0;
static volatile uint32_t g_f2838xif_pbuf_icmp_echo_for_us_count = 0;
static volatile uint32_t g_f2838xif_tx_pbuf_eth_type = 0;
static volatile uint32_t g_f2838xif_tx_pbuf_len = 0;
static volatile uint32_t g_f2838xif_tx_dst_mac_high = 0;
static volatile uint32_t g_f2838xif_tx_dst_mac_low = 0;
static volatile uint32_t g_f2838xif_tx_src_mac_high = 0;
static volatile uint32_t g_f2838xif_tx_src_mac_low = 0;
static volatile uint32_t g_f2838xif_tx_ipv4_src_ip = 0;
static volatile uint32_t g_f2838xif_tx_ipv4_dst_ip = 0;
static volatile uint32_t g_f2838xif_tx_ipv4_proto = 0;
static volatile uint32_t g_f2838xif_tx_icmp_type = 0;
static volatile uint32_t g_f2838xif_tx_arp_count = 0;
static volatile uint32_t g_f2838xif_tx_arp_dst_mac_high = 0;
static volatile uint32_t g_f2838xif_tx_arp_dst_mac_low = 0;
static volatile uint32_t g_f2838xif_tx_arp_src_mac_high = 0;
static volatile uint32_t g_f2838xif_tx_arp_src_mac_low = 0;
static volatile uint32_t g_f2838xif_tx_arp_target_ip = 0;
static volatile uint32_t g_f2838xif_tx_ipv4_count = 0;
static volatile uint32_t g_f2838xif_tx_ipv4_send_ok_count = 0;
static volatile uint32_t g_f2838xif_tx_ipv4_send_fail_count = 0;
static volatile uint32_t g_f2838xif_tx_ipv4_dst_mac_high = 0;
static volatile uint32_t g_f2838xif_tx_ipv4_dst_mac_low = 0;
static volatile uint32_t g_f2838xif_tx_ipv4_src_mac_high = 0;
static volatile uint32_t g_f2838xif_tx_ipv4_src_mac_low = 0;
static volatile uint32_t g_f2838xif_tx_ipv4_last_src_ip = 0;
static volatile uint32_t g_f2838xif_tx_ipv4_last_dst_ip = 0;
static volatile uint32_t g_f2838xif_tx_ipv4_last_proto = 0;
static volatile uint32_t g_f2838xif_tx_ipv4_last_icmp_type = 0;
static volatile uint32_t g_f2838xif_tx_pending_eth_type = 0;
static volatile uint32_t g_f2838xif_tx_pending_ipv4_proto = 0;
static volatile uint32_t g_f2838xif_tx_pending_icmp_type = 0;
volatile uint32_t g_f2838xif_tx_stage = 0;
static volatile uint32_t g_f2838xif_tx_pop_null_count = 0;
static volatile uint32_t g_f2838xif_tx_pop_null_freeq_count = 0;
volatile uint32_t g_f2838xif_tx_last_error = 0;
static volatile uint32_t g_f2838xif_tx_last_chain_len = 0;
static volatile uint32_t g_f2838xif_tx_freeq_count = 0;
static volatile uint32_t g_f2838xif_tx_no_desc_count = 0;
static volatile uint32_t g_f2838xif_tx_null_pbuf_count = 0;
static volatile uint32_t g_f2838xif_linkoutput_error_count = 0;
static volatile uint32_t g_f2838xif_tx_inuse_spin_count = 0;
volatile uint32_t g_f2838xif_raw_arp_reply_count = 0;
volatile uint32_t g_f2838xif_raw_icmp_reply_count = 0;
volatile uint32_t g_f2838xif_raw_reply_busy_count = 0;
volatile uint32_t g_f2838xif_raw_reply_drop_count = 0;
volatile uint32_t g_f2838xif_pc_arp_request_count = 0;
volatile uint32_t g_f2838xif_pc_icmp_echo_request_count = 0;
volatile uint32_t g_f2838xif_ping_attempt_count = 0;
volatile uint32_t g_f2838xif_ping_success_count = 0;
volatile uint32_t g_f2838xif_ping_failure_count = 0;
volatile uint32_t g_f2838xif_ping_success_rate_x100 = 0;
volatile uint32_t g_f2838xif_ping_failure_rate_x100 = 0;
volatile uint32_t g_f2838xif_pc_tcp_request_count = 0;
static volatile uint32_t g_f2838xif_pc_udp_request_count = 0;
static volatile uint32_t g_f2838xif_pc_other_ipv4_request_count = 0;
static volatile uint32_t g_f2838xif_pc_last_request_proto = 0;
static volatile uint32_t g_f2838xif_pc_last_request_ip = 0;
static volatile uint32_t g_f2838xif_pc_last_request_icmp_type = 0;
#if CM_ETH_BRINGUP_HTTP
volatile uint32_t g_f2838xif_raw_tcp_syn_request_count = 0;
volatile uint32_t g_f2838xif_raw_tcp_synack_count = 0;
volatile uint32_t g_f2838xif_raw_tcp_http_request_count = 0;
volatile uint32_t g_f2838xif_raw_tcp_http_response_count = 0;
volatile uint32_t g_f2838xif_raw_tcp_stage = 0;
volatile uint32_t g_f2838xif_raw_tcp_last = 0;
volatile uint32_t g_f2838xif_raw_tcp_error = 0;
volatile uint32_t g_f2838xif_raw_tcp_synack_fail_count = 0;
volatile uint32_t g_f2838xif_raw_tcp_ack_only_count = 0;
volatile uint32_t g_f2838xif_raw_tcp_fin_ack_count = 0;
volatile uint32_t g_f2838xif_raw_tcp_rst_count = 0;
#endif

extern volatile uint32_t g_lwip_config_ip;

#pragma DATA_ALIGN(f2838xif_TxBringupBuffer, 4U)
static uint8_t f2838xif_TxBringupBuffer[1536U];
static uint32_t f2838xif_TxBringupDescInUse;
static uint32_t f2838xif_TxBringupDescInUseTicks;
static uint32_t f2838xif_TxPending;
static uint32_t f2838xif_TxPendingLen;

#define F2838XIF_RAW_TX_SLOTS 32U
#define F2838XIF_RAW_TX_BUFFER_SIZE 128U

#pragma DATA_ALIGN(f2838xif_RawTxBuffer, 4U)
static uint8_t f2838xif_RawTxBuffer[F2838XIF_RAW_TX_SLOTS][F2838XIF_RAW_TX_BUFFER_SIZE];
#if CM_ETH_BRINGUP_HTTP
static uint8_t f2838xif_RawTcpBuildFrame[256U];
#endif
static Ethernet_Pkt_Desc f2838xif_RawTxDesc[F2838XIF_RAW_TX_SLOTS];
static uint32_t f2838xif_RawTxNextSlot;
#if CM_ETH_BRINGUP_HTTP
static uint16_t f2838xif_IpId;
static uint32_t f2838xif_TcpServerSeq = 0x28380000U;
static uint32_t f2838xif_TcpServerNextSeq = 0x28380000U;
static uint16_t f2838xif_TcpClientPort;
static uint32_t f2838xif_TcpClientIp;
static uint8_t f2838xif_TcpClientMac[6U];
#endif

Ethernet_Handle emac_handle;
extern Ethernet_Device Ethernet_device_struct;

void f2838xif_noteTxComplete(void);

static uint32_t f2838xif_read_be32(const uint8_t *p)
{
    return (((uint32_t)p[0]) << 24) |
           (((uint32_t)p[1]) << 16) |
           (((uint32_t)p[2]) << 8) |
           ((uint32_t)p[3]);
}

static void f2838xif_write_be16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static void f2838xif_write_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static void f2838xif_update_ping_rates(void)
{
    if(g_f2838xif_ping_attempt_count == 0U)
    {
        g_f2838xif_ping_success_rate_x100 = 0U;
        g_f2838xif_ping_failure_rate_x100 = 0U;
        return;
    }

    g_f2838xif_ping_success_rate_x100 =
        (g_f2838xif_ping_success_count * 10000U) /
        g_f2838xif_ping_attempt_count;
    g_f2838xif_ping_failure_rate_x100 =
        (g_f2838xif_ping_failure_count * 10000U) /
        g_f2838xif_ping_attempt_count;
}

static uint16_t f2838xif_checksum(const uint8_t *data, uint32_t len)
{
    uint32_t sum = 0U;

    while(len > 1U)
    {
        sum += ((((uint32_t)data[0]) << 8) | (uint32_t)data[1]);
        data += 2;
        len -= 2U;
    }

    if(len != 0U)
    {
        sum += (((uint32_t)data[0]) << 8);
    }

    while((sum >> 16) != 0U)
    {
        sum = (sum & 0xFFFFU) + (sum >> 16);
    }

    return (uint16_t)(~sum);
}

static void f2838xif_capture_raw_tx_info(uint8_t *data, uint32_t len)
{
    g_f2838xif_tx_pbuf_len = len;
    g_f2838xif_tx_pbuf_eth_type =
        (((uint32_t)data[12]) << 8) | ((uint32_t)data[13]);
    g_f2838xif_tx_pending_eth_type = g_f2838xif_tx_pbuf_eth_type;
    g_f2838xif_tx_dst_mac_high =
        (((uint32_t)data[0]) << 8) | ((uint32_t)data[1]);
    g_f2838xif_tx_dst_mac_low =
        (((uint32_t)data[2]) << 24) |
        (((uint32_t)data[3]) << 16) |
        (((uint32_t)data[4]) << 8) |
        ((uint32_t)data[5]);
    g_f2838xif_tx_src_mac_high =
        (((uint32_t)data[6]) << 8) | ((uint32_t)data[7]);
    g_f2838xif_tx_src_mac_low =
        (((uint32_t)data[8]) << 24) |
        (((uint32_t)data[9]) << 16) |
        (((uint32_t)data[10]) << 8) |
        ((uint32_t)data[11]);

    if((g_f2838xif_tx_pbuf_eth_type == 0x0800U) && (len >= 35U))
    {
        ++g_f2838xif_tx_ipv4_count;
        g_f2838xif_tx_ipv4_proto = data[23];
        g_f2838xif_tx_ipv4_src_ip = f2838xif_read_be32(&data[26]);
        g_f2838xif_tx_ipv4_dst_ip = f2838xif_read_be32(&data[30]);
        g_f2838xif_tx_icmp_type = data[34];
        g_f2838xif_tx_pending_ipv4_proto = g_f2838xif_tx_ipv4_proto;
        g_f2838xif_tx_pending_icmp_type = g_f2838xif_tx_icmp_type;
    }
}

static uint32_t f2838xif_queue_raw_reply(uint8_t *frame, uint32_t len)
{
    uint32_t slot;
    Ethernet_Pkt_Desc *pktDescPtr;

    if(len > F2838XIF_RAW_TX_BUFFER_SIZE)
    {
        ++g_f2838xif_raw_reply_busy_count;
        return 0U;
    }

    slot = f2838xif_RawTxNextSlot;
    f2838xif_RawTxNextSlot = (f2838xif_RawTxNextSlot + 1U) % F2838XIF_RAW_TX_SLOTS;

    memcpy(f2838xif_RawTxBuffer[slot], frame, len);
    pktDescPtr = &f2838xif_RawTxDesc[slot];
    pktDescPtr->nextPacketDesc = NULL;
    pktDescPtr->dataBuffer = f2838xif_RawTxBuffer[slot];
    pktDescPtr->pAppData = NULL;
    pktDescPtr->dataBuffer2 = NULL;
    pktDescPtr->bufferLength = len;
    pktDescPtr->buffer2Length = 0U;
    pktDescPtr->pktChannel = ETHERNET_DMA_CHANNEL_NUM_0;
    pktDescPtr->pktLength = len;
    pktDescPtr->flags = ETHERNET_PKT_FLAG_SOP | ETHERNET_PKT_FLAG_EOP;
    pktDescPtr->numPktFrags = 1U;
    pktDescPtr->dataOffset = 0U;
    pktDescPtr->validLength = len;
    pktDescPtr->timeStampLow = 0U;
    pktDescPtr->timeStampHigh = 0U;
    pktDescPtr->nextBufferDiscarded = 0U;
    pktDescPtr->extendedFlags = 0U;
    pktDescPtr->innerVlanTag = 0U;
    pktDescPtr->mssTso = 0U;
    pktDescPtr->vlanTag = 0U;

    f2838xif_capture_raw_tx_info(f2838xif_RawTxBuffer[slot], len);
    ++g_f2838xif_transmit_count;
    g_f2838xif_tx_stage = 704U;

    if(Ethernet_sendPacket(emac_handle, pktDescPtr) != ETHERNET_RET_SUCCESS)
    {
        ++g_f2838xif_transmit_fail_count;
        if(g_f2838xif_tx_pending_eth_type == 0x0800U)
        {
            ++g_f2838xif_tx_ipv4_send_fail_count;
        }
        g_f2838xif_tx_last_error = (uint32_t)ERR_MEM;
        g_f2838xif_tx_stage = 705U;
        return 0U;
    }

    ++g_f2838xif_transmit_ok_count;
    if(g_f2838xif_tx_pending_eth_type == 0x0800U)
    {
        ++g_f2838xif_tx_ipv4_send_ok_count;
    }
    g_f2838xif_tx_last_error = (uint32_t)ERR_OK;
    g_f2838xif_tx_stage = 706U;
    LINK_STATS_INC(link.xmit);

    return 1U;
}

#if CM_ETH_BRINGUP_HTTP
static uint16_t f2838xif_tcp_checksum(uint8_t *ipHeader, uint8_t *tcpHeader,
                                      uint32_t tcpLen)
{
    uint32_t sum = 0U;
    uint32_t i;

    sum += ((((uint32_t)ipHeader[12]) << 8) | (uint32_t)ipHeader[13]);
    sum += ((((uint32_t)ipHeader[14]) << 8) | (uint32_t)ipHeader[15]);
    sum += ((((uint32_t)ipHeader[16]) << 8) | (uint32_t)ipHeader[17]);
    sum += ((((uint32_t)ipHeader[18]) << 8) | (uint32_t)ipHeader[19]);
    sum += 6U;
    sum += tcpLen;

    for(i = 0U; (i + 1U) < tcpLen; i += 2U)
    {
        sum += ((((uint32_t)tcpHeader[i]) << 8) | (uint32_t)tcpHeader[i + 1U]);
    }
    if((tcpLen & 1U) != 0U)
    {
        sum += (((uint32_t)tcpHeader[tcpLen - 1U]) << 8);
    }

    while((sum >> 16) != 0U)
    {
        sum = (sum & 0xFFFFU) + (sum >> 16);
    }

    return (uint16_t)(~sum);
}

static void f2838xif_finish_ipv4_tcp(uint8_t *frame, uint32_t ipLen)
{
    uint8_t *ip = &frame[14];
    uint8_t *tcp = &frame[34];
    uint32_t tcpLen = ipLen - 20U;

    f2838xif_write_be16(&ip[2], (uint16_t)ipLen);
    f2838xif_write_be16(&ip[4], ++f2838xif_IpId);
    ip[6] = 0U;
    ip[7] = 0U;
    ip[8] = 255U;
    ip[9] = 6U;
    ip[10] = 0U;
    ip[11] = 0U;
    f2838xif_write_be16(&ip[10], f2838xif_checksum(ip, 20U));

    tcp[16] = 0U;
    tcp[17] = 0U;
    f2838xif_write_be16(&tcp[16], f2838xif_tcp_checksum(ip, tcp, tcpLen));
}

static uint32_t f2838xif_send_raw_tcp(uint8_t *dstMac, uint32_t dstIp,
                                      uint16_t dstPort, uint16_t srcPort,
                                      uint32_t seq, uint32_t ack,
                                      uint8_t flags,
                                      const uint8_t *payload,
                                      uint32_t payloadLen)
{
    uint8_t *frame = f2838xif_RawTcpBuildFrame;
    uint32_t tcpLen = 20U + payloadLen;
    uint32_t ipLen = 20U + tcpLen;
    uint32_t frameLen = 14U + ipLen;

    g_f2838xif_raw_tcp_stage = 100U;

    if(frameLen < 60U)
    {
        frameLen = 60U;
    }
    if(frameLen > sizeof(f2838xif_RawTcpBuildFrame))
    {
        g_f2838xif_raw_tcp_stage = 101U;
        return 0U;
    }

    g_f2838xif_raw_tcp_stage = 102U;
    memset(frame, 0, sizeof(f2838xif_RawTcpBuildFrame));
    memcpy(&frame[0], dstMac, 6U);
    frame[6] = 0xA8U;
    frame[7] = 0x63U;
    frame[8] = 0xF2U;
    frame[9] = 0x00U;
    frame[10] = 0x00U;
    frame[11] = 0x80U;
    frame[12] = 0x08U;
    frame[13] = 0x00U;

    frame[14] = 0x45U;
    frame[15] = 0x00U;
    f2838xif_write_be32(&frame[26], g_lwip_config_ip);
    f2838xif_write_be32(&frame[30], dstIp);

    f2838xif_write_be16(&frame[34], srcPort);
    f2838xif_write_be16(&frame[36], dstPort);
    f2838xif_write_be32(&frame[38], seq);
    f2838xif_write_be32(&frame[42], ack);
    frame[46] = 0x50U;
    frame[47] = flags;
    f2838xif_write_be16(&frame[48], 4096U);
    if((payload != NULL) && (payloadLen != 0U))
    {
        memcpy(&frame[54], payload, payloadLen);
    }

    g_f2838xif_raw_tcp_stage = 103U;
    f2838xif_finish_ipv4_tcp(frame, ipLen);
    g_f2838xif_raw_tcp_stage = 104U;
    if(f2838xif_queue_raw_reply(frame, frameLen) != 0U)
    {
        g_f2838xif_raw_tcp_stage = 105U;
        return 1U;
    }

    g_f2838xif_raw_tcp_stage = 106U;
    return 0U;
}
#endif

static uint32_t f2838xif_try_raw_arp_reply(struct netif *netif, struct pbuf *p)
{
    uint8_t frame[60];
    uint8_t *data;

    if((p == NULL) || (p->payload == NULL) || (p->len < 42U))
    {
        return 0U;
    }

    data = (uint8_t *)p->payload;
    if((((uint32_t)data[12] << 8) | (uint32_t)data[13]) != 0x0806U)
    {
        return 0U;
    }
    if((((uint32_t)data[20] << 8) | (uint32_t)data[21]) != 1U)
    {
        return 0U;
    }
    if(f2838xif_read_be32(&data[38]) != g_lwip_config_ip)
    {
        return 0U;
    }

    ++g_f2838xif_pc_arp_request_count;
    g_f2838xif_pc_last_request_proto = 0x0806U;
    g_f2838xif_pc_last_request_ip = f2838xif_read_be32(&data[28]);

    memset(frame, 0, sizeof(frame));
    memcpy(&frame[0], &data[6], 6U);
    memcpy(&frame[6], netif->hwaddr, 6U);
    frame[12] = 0x08U;
    frame[13] = 0x06U;
    memcpy(&frame[14], &data[14], 8U);
    frame[20] = 0x00U;
    frame[21] = 0x02U;
    memcpy(&frame[22], netif->hwaddr, 6U);
    f2838xif_write_be32(&frame[28], g_lwip_config_ip);
    memcpy(&frame[32], &data[6], 6U);
    memcpy(&frame[38], &data[28], 4U);

    if(f2838xif_queue_raw_reply(frame, sizeof(frame)) != 0U)
    {
        ++g_f2838xif_raw_arp_reply_count;
        return 1U;
    }

    ++g_f2838xif_raw_reply_drop_count;
    return 1U;
}

static uint32_t f2838xif_try_raw_icmp_reply(struct netif *netif, struct pbuf *p)
{
    uint8_t frame[98];
    uint8_t *data;
    uint32_t ipHeaderLen;
    uint32_t totalLen;
    uint32_t frameLen;
    uint32_t icmpLen;
    uint32_t srcIp;

    if((p == NULL) || (p->payload == NULL) || (p->len < 42U))
    {
        return 0U;
    }

    data = (uint8_t *)p->payload;
    if((((uint32_t)data[12] << 8) | (uint32_t)data[13]) != 0x0800U)
    {
        return 0U;
    }
    if((data[14] >> 4) != 4U)
    {
        return 0U;
    }

    ipHeaderLen = ((uint32_t)(data[14] & 0x0FU)) * 4U;
    totalLen = (((uint32_t)data[16]) << 8) | (uint32_t)data[17];
    if((ipHeaderLen < 20U) || (totalLen < (ipHeaderLen + 8U)) ||
       ((14U + totalLen) > p->len) || ((14U + totalLen) > sizeof(frame)))
    {
        return 0U;
    }
    if((data[23] != 1U) || (f2838xif_read_be32(&data[30]) != g_lwip_config_ip) ||
       (data[14U + ipHeaderLen] != 8U))
    {
        return 0U;
    }

    ++g_f2838xif_pc_icmp_echo_request_count;
    ++g_f2838xif_ping_attempt_count;
    g_f2838xif_pc_last_request_proto = 1U;
    g_f2838xif_pc_last_request_ip = f2838xif_read_be32(&data[26]);
    g_f2838xif_pc_last_request_icmp_type = data[14U + ipHeaderLen];

    frameLen = 14U + totalLen;
    memcpy(frame, data, frameLen);

    memcpy(&frame[0], &data[6], 6U);
    memcpy(&frame[6], netif->hwaddr, 6U);

    srcIp = f2838xif_read_be32(&data[26]);
    f2838xif_write_be32(&frame[26], g_lwip_config_ip);
    f2838xif_write_be32(&frame[30], srcIp);
    frame[22] = 255U;
    frame[24] = 0U;
    frame[25] = 0U;
    f2838xif_write_be16(&frame[24], f2838xif_checksum(&frame[14], ipHeaderLen));

    frame[14U + ipHeaderLen] = 0U;
    frame[14U + ipHeaderLen + 2U] = 0U;
    frame[14U + ipHeaderLen + 3U] = 0U;
    icmpLen = totalLen - ipHeaderLen;
    f2838xif_write_be16(&frame[14U + ipHeaderLen + 2U],
                        f2838xif_checksum(&frame[14U + ipHeaderLen], icmpLen));

    if(f2838xif_queue_raw_reply(frame, frameLen) != 0U)
    {
        ++g_f2838xif_raw_icmp_reply_count;
        ++g_f2838xif_ping_success_count;
        f2838xif_update_ping_rates();
        return 1U;
    }

    ++g_f2838xif_raw_reply_drop_count;
    ++g_f2838xif_ping_failure_count;
    f2838xif_update_ping_rates();
    return 1U;
}

#if CM_ETH_BRINGUP_HTTP
static uint32_t f2838xif_try_raw_tcp_http(struct netif *netif, struct pbuf *p)
{
    static const uint8_t httpResponse[] =
        "HTTP/1.0 200 OK\r\n"
        "Content-Length: 3\r\n"
        "\r\n"
        "OK\n";
    uint8_t *data;
    uint32_t ipHeaderLen;
    uint32_t tcpHeaderLen;
    uint32_t totalLen;
    uint32_t tcpPayloadLen;
    uint32_t clientSeq;
    uint16_t srcPort;
    uint16_t dstPort;
    uint8_t flags;

    g_f2838xif_raw_tcp_stage = 1U;

    if((p == NULL) || (p->payload == NULL) || (p->len < 54U))
    {
        g_f2838xif_raw_tcp_error =
            (1UL << 24) | ((p == NULL) ? 0U : (uint32_t)p->len);
        return 0U;
    }

    data = (uint8_t *)p->payload;
    g_f2838xif_raw_tcp_stage = 2U;
    if((((uint32_t)data[12] << 8) | (uint32_t)data[13]) != 0x0800U)
    {
        g_f2838xif_raw_tcp_error = 2UL << 24;
        return 0U;
    }
    if((data[14] >> 4) != 4U)
    {
        g_f2838xif_raw_tcp_error = 3UL << 24;
        return 0U;
    }

    ipHeaderLen = ((uint32_t)(data[14] & 0x0FU)) * 4U;
    totalLen = (((uint32_t)data[16]) << 8) | (uint32_t)data[17];
    g_f2838xif_raw_tcp_stage = 3U;
    if((ipHeaderLen < 20U) || (data[23] != 6U) ||
       (f2838xif_read_be32(&data[30]) != g_lwip_config_ip) ||
       ((14U + totalLen) > p->len))
    {
        g_f2838xif_raw_tcp_error =
            (4UL << 24) | (((uint32_t)data[23]) << 16) |
            (totalLen & 0xFFFFU);
        return 0U;
    }

    srcPort = (uint16_t)((((uint32_t)data[14U + ipHeaderLen]) << 8) |
                         (uint32_t)data[14U + ipHeaderLen + 1U]);
    dstPort = (uint16_t)((((uint32_t)data[14U + ipHeaderLen + 2U]) << 8) |
                         (uint32_t)data[14U + ipHeaderLen + 3U]);
    g_f2838xif_raw_tcp_stage = 4U;
    if(dstPort != 80U)
    {
        g_f2838xif_raw_tcp_error = (5UL << 24) | (uint32_t)dstPort;
        return 0U;
    }

    tcpHeaderLen = ((uint32_t)(data[14U + ipHeaderLen + 12U] >> 4)) * 4U;
    if((tcpHeaderLen < 20U) || (totalLen < (ipHeaderLen + tcpHeaderLen)))
    {
        g_f2838xif_raw_tcp_error =
            (6UL << 24) | ((tcpHeaderLen & 0xFFU) << 16) |
            (totalLen & 0xFFFFU);
        return 0U;
    }

    flags = data[14U + ipHeaderLen + 13U];
    g_f2838xif_raw_tcp_stage = 5U;
    clientSeq = f2838xif_read_be32(&data[14U + ipHeaderLen + 4U]);
    tcpPayloadLen = totalLen - ipHeaderLen - tcpHeaderLen;
    g_f2838xif_raw_tcp_last =
        (((uint32_t)flags) << 24) |
        (((uint32_t)dstPort & 0xFFFFU) << 8) |
        (tcpPayloadLen & 0xFFU);

    g_f2838xif_raw_tcp_error = 0U;

    if((flags & 0x02U) != 0U)
    {
        g_f2838xif_raw_tcp_stage = 60U;
        ++g_f2838xif_raw_tcp_syn_request_count;
        f2838xif_TcpServerSeq += 0x100U;
        f2838xif_TcpServerNextSeq = f2838xif_TcpServerSeq + 1U;
        f2838xif_TcpClientPort = srcPort;
        f2838xif_TcpClientIp = f2838xif_read_be32(&data[26]);
        memcpy(f2838xif_TcpClientMac, &data[6], 6U);
        g_f2838xif_raw_tcp_stage = 61U;
        if(f2838xif_send_raw_tcp(&data[6], f2838xif_TcpClientIp,
                                 srcPort, 80U,
                                 f2838xif_TcpServerSeq,
                                 clientSeq + 1U,
                                 0x12U, NULL, 0U) != 0U)
        {
            g_f2838xif_raw_tcp_stage = 62U;
            ++g_f2838xif_raw_tcp_synack_count;
        }
        else
        {
            g_f2838xif_raw_tcp_stage = 63U;
            ++g_f2838xif_raw_tcp_synack_fail_count;
            ++g_f2838xif_raw_reply_drop_count;
        }
        return 1U;
    }

    if((srcPort == f2838xif_TcpClientPort) &&
       (f2838xif_read_be32(&data[26]) == f2838xif_TcpClientIp) &&
       ((flags & 0x04U) != 0U))
    {
        g_f2838xif_raw_tcp_stage = 90U;
        ++g_f2838xif_raw_tcp_rst_count;
        f2838xif_TcpClientPort = 0U;
        f2838xif_TcpClientIp = 0U;
        return 1U;
    }

    if((tcpPayloadLen != 0U) && (srcPort == f2838xif_TcpClientPort) &&
       (f2838xif_read_be32(&data[26]) == f2838xif_TcpClientIp))
    {
        uint32_t httpLen = (uint32_t)(sizeof(httpResponse) - 1U);
        g_f2838xif_raw_tcp_stage = 70U;
        ++g_f2838xif_raw_tcp_http_request_count;
        if(f2838xif_send_raw_tcp(f2838xif_TcpClientMac, f2838xif_TcpClientIp,
                                 f2838xif_TcpClientPort, 80U,
                                 f2838xif_TcpServerSeq + 1U,
                                 clientSeq + tcpPayloadLen,
                                 0x19U, httpResponse,
                                 httpLen) != 0U)
        {
            f2838xif_TcpServerNextSeq =
                f2838xif_TcpServerSeq + 1U + httpLen + 1U;
            ++g_f2838xif_raw_tcp_http_response_count;
        }
        else
        {
            ++g_f2838xif_raw_reply_drop_count;
        }
        return 1U;
    }

    if((srcPort == f2838xif_TcpClientPort) &&
       (f2838xif_read_be32(&data[26]) == f2838xif_TcpClientIp))
    {
        if((flags & 0x01U) != 0U)
        {
            g_f2838xif_raw_tcp_stage = 85U;
            if(f2838xif_send_raw_tcp(f2838xif_TcpClientMac,
                                     f2838xif_TcpClientIp,
                                     f2838xif_TcpClientPort, 80U,
                                     f2838xif_TcpServerNextSeq,
                                     clientSeq + 1U,
                                     0x10U, NULL, 0U) != 0U)
            {
                ++g_f2838xif_raw_tcp_fin_ack_count;
            }
            else
            {
                ++g_f2838xif_raw_reply_drop_count;
            }
            return 1U;
        }

        g_f2838xif_raw_tcp_stage = 80U;
        ++g_f2838xif_raw_tcp_ack_only_count;
        return 1U;
    }

    return 0U;
}

static uint32_t f2838xif_is_raw_tcp_candidate(struct pbuf *p)
{
    uint8_t *data;
    uint32_t ipHeaderLen;
    uint32_t totalLen;

    if((p == NULL) || (p->payload == NULL) || (p->len < 54U))
    {
        return 0U;
    }

    data = (uint8_t *)p->payload;
    if(((((uint32_t)data[12]) << 8) | (uint32_t)data[13]) != 0x0800U)
    {
        return 0U;
    }
    if((data[14] >> 4) != 4U)
    {
        return 0U;
    }

    ipHeaderLen = ((uint32_t)(data[14] & 0x0FU)) * 4U;
    totalLen = (((uint32_t)data[16]) << 8) | (uint32_t)data[17];
    if((ipHeaderLen < 20U) || (data[23] != 6U) ||
       (f2838xif_read_be32(&data[30]) != g_lwip_config_ip) ||
       ((14U + totalLen) > p->len))
    {
        return 0U;
    }

    return 1U;
}
#endif

static void f2838xif_capture_rx_pbuf_info(struct pbuf *p)
{
    uint8_t *data;
    uint32_t ethType;

    if((p == NULL) || (p->payload == NULL) || (p->len < 14U))
    {
        return;
    }

    data = (uint8_t *)p->payload;
    ethType = (((uint32_t)data[12]) << 8) | ((uint32_t)data[13]);
    g_f2838xif_rx_pbuf_eth_type = ethType;

    if((ethType == 0x0806U) && (p->len >= 42U))
    {
        uint32_t ip = f2838xif_read_be32(&data[38]);
        g_f2838xif_rx_pbuf_arp_target_ip = ip;
        if(ip == g_lwip_config_ip)
        {
            ++g_f2838xif_pbuf_arp_for_us_count;
        }
    }
    else if((ethType == 0x0800U) && (p->len >= 34U))
    {
        uint32_t ip = f2838xif_read_be32(&data[30]);
        uint32_t proto = data[23];
        g_f2838xif_rx_pbuf_ipv4_dst_ip = ip;
        g_f2838xif_rx_pbuf_ipv4_proto = proto;
        if(ip == g_lwip_config_ip)
        {
            ++g_f2838xif_pbuf_ipv4_for_us_count;
            g_f2838xif_pc_last_request_proto = proto;
            g_f2838xif_pc_last_request_ip = f2838xif_read_be32(&data[26]);
            if((proto == 1U) && (p->len >= 35U))
            {
                g_f2838xif_rx_pbuf_icmp_type = data[34];
                if(data[34] == 8U)
                {
                    ++g_f2838xif_pbuf_icmp_echo_for_us_count;
                }
            }
            else if(proto == 6U)
            {
                ++g_f2838xif_pc_tcp_request_count;
            }
            else if(proto == 17U)
            {
                ++g_f2838xif_pc_udp_request_count;
            }
            else
            {
                ++g_f2838xif_pc_other_ipv4_request_count;
            }
        }
    }
}

static void f2838xif_capture_tx_pbuf_info(struct pbuf *p)
{
    uint8_t *data;

    if((p == NULL) || (p->payload == NULL))
    {
        return;
    }

    g_f2838xif_tx_pbuf_len = p->tot_len;

    if(p->len >= 14U)
    {
        data = (uint8_t *)p->payload;
        g_f2838xif_tx_dst_mac_high =
            (((uint32_t)data[0]) << 8) | ((uint32_t)data[1]);
        g_f2838xif_tx_dst_mac_low =
            (((uint32_t)data[2]) << 24) |
            (((uint32_t)data[3]) << 16) |
            (((uint32_t)data[4]) << 8) |
            ((uint32_t)data[5]);
        g_f2838xif_tx_src_mac_high =
            (((uint32_t)data[6]) << 8) | ((uint32_t)data[7]);
        g_f2838xif_tx_src_mac_low =
            (((uint32_t)data[8]) << 24) |
            (((uint32_t)data[9]) << 16) |
            (((uint32_t)data[10]) << 8) |
            ((uint32_t)data[11]);
        g_f2838xif_tx_pbuf_eth_type =
            (((uint32_t)data[12]) << 8) | ((uint32_t)data[13]);
        if((g_f2838xif_tx_pbuf_eth_type == 0x0806U) && (p->len >= 42U))
        {
            ++g_f2838xif_tx_arp_count;
            g_f2838xif_tx_arp_dst_mac_high = g_f2838xif_tx_dst_mac_high;
            g_f2838xif_tx_arp_dst_mac_low = g_f2838xif_tx_dst_mac_low;
            g_f2838xif_tx_arp_src_mac_high = g_f2838xif_tx_src_mac_high;
            g_f2838xif_tx_arp_src_mac_low = g_f2838xif_tx_src_mac_low;
            g_f2838xif_tx_arp_target_ip = f2838xif_read_be32(&data[38]);
        }
        if((g_f2838xif_tx_pbuf_eth_type == 0x0800U) && (p->len >= 35U))
        {
            ++g_f2838xif_tx_ipv4_count;
            g_f2838xif_tx_ipv4_dst_mac_high = g_f2838xif_tx_dst_mac_high;
            g_f2838xif_tx_ipv4_dst_mac_low = g_f2838xif_tx_dst_mac_low;
            g_f2838xif_tx_ipv4_src_mac_high = g_f2838xif_tx_src_mac_high;
            g_f2838xif_tx_ipv4_src_mac_low = g_f2838xif_tx_src_mac_low;
            g_f2838xif_tx_ipv4_proto = data[23];
            g_f2838xif_tx_ipv4_src_ip = f2838xif_read_be32(&data[26]);
            g_f2838xif_tx_ipv4_dst_ip = f2838xif_read_be32(&data[30]);
            g_f2838xif_tx_icmp_type = data[34];
            g_f2838xif_tx_ipv4_last_proto = g_f2838xif_tx_ipv4_proto;
            g_f2838xif_tx_ipv4_last_src_ip = g_f2838xif_tx_ipv4_src_ip;
            g_f2838xif_tx_ipv4_last_dst_ip = g_f2838xif_tx_ipv4_dst_ip;
            g_f2838xif_tx_ipv4_last_icmp_type = g_f2838xif_tx_icmp_type;
        }
        g_f2838xif_tx_pending_eth_type = g_f2838xif_tx_pbuf_eth_type;
        g_f2838xif_tx_pending_ipv4_proto = g_f2838xif_tx_ipv4_proto;
        g_f2838xif_tx_pending_icmp_type = g_f2838xif_tx_icmp_type;
    }
}

#define DRIVER_STATS_INC(x) do{ g_sDriverStats.ui32##x++; } while(0)
#define DRIVER_STATS_DEC(x) do{ g_sDriverStats.ui32##x--; } while(0)
#define DRIVER_STATS_ADD(x, inc) do{ g_sDriverStats.ui32##x += inc; } while(0)
#define DRIVER_STATS_SUB(x, dec) do{ g_sDriverStats.ui32##x -= dec; } while(0)

/*
 * Creating a queue that maps an ethernet packet descriptor with
 * the corresponding pbuf. It is useful to free up the allocated pbuf memory
 * after the packet has been sent.
 */

#define NUM_PACKET_DESC_TX_APPLICATION  20
#define NUM_PACKET_DESC_RX_APPLICATION  10

/**
 * A macro which determines whether a pointer is within the SRAM address
 * space and, hence, points to a buffer that the Ethernet MAC can directly
 * DMA from.
 */
#define PTR_SAFE_FOR_EMAC_DMA(ptr) (((uint32_t)(ptr) >= 0x2000800) &&   \
                                    ((uint32_t)(ptr) < 0x2000FFFF))

/**
 * Global variable for this interface's private data.  Needed to allow
 * the interrupt handlers access to this information outside of the
 * context of the lwIP netif.
 *
 */

struct f2838xif f2838xif_data;

/*
 *Tx and Rx free queues to hold the free packet descriptors
 */
static Ethernet_PKT_Queue_T RxPktFreeQ, TxPktFreeQ;
static Ethernet_Pkt_Desc f2838xif_PktDesc[NUM_PACKET_DESC_RX_APPLICATION +
                                    NUM_PACKET_DESC_TX_APPLICATION];
static Ethernet_Pkt_Desc f2838xif_TxBringupDesc;

static Ethernet_Pkt_Desc *f2838xif_popTxBringupDesc(void)
{
    g_f2838xif_tx_stage = 301U;
    if(f2838xif_TxBringupDescInUse != 0U)
    {
        TxPktFreeQ.count = 0U;
        g_f2838xif_tx_stage = 303U;
        return NULL;
    }

    g_f2838xif_tx_stage = 302U;
    f2838xif_TxBringupDescInUse = 1U;
    TxPktFreeQ.count = 0U;
    g_f2838xif_tx_stage = 308U;
    return &f2838xif_TxBringupDesc;
}

/**
 * In this function, the hardware should be initialized.
 * Called from f2838xif_init().
 *
 * @param netif the already initialized lwip network interface structure
 *        for this ethernetif
 */
static void
f2838xif_hwinit(struct netif *netif)
{
    uint32_t mac_low,mac_high;
    uint8_t *pucTemp;

    /* set MAC hardware address length */
    netif->hwaddr_len = ETHARP_HWADDR_LEN;

    /* set MAC address */
    Ethernet_getMACAddr(EMAC_BASE, 0, &mac_high, &mac_low);

    pucTemp = (uint8_t *)&mac_low;
    netif->hwaddr[0] = pucTemp[0];
    netif->hwaddr[1] = pucTemp[1];
    netif->hwaddr[2] = pucTemp[2];
    netif->hwaddr[3] = pucTemp[3];

    pucTemp = (uint8_t *)&mac_high;
    netif->hwaddr[4] = pucTemp[0];
    netif->hwaddr[5] = pucTemp[1];

    /* maximum transfer unit */
    netif->mtu = 1500;

    /* device capabilities */
    /* don't set NETIF_FLAG_ETHARP if this device is not an ethernet one */
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP | NETIF_FLAG_IGMP;
}

/*
f2838xif_freePktDesc

This function is called when a Tx packet Descriptor has to be freed.
*/
static void f2838xif_freePktDesc(Ethernet_Pkt_Desc * pktDescPtr)
{
    if(pktDescPtr == NULL)
    {
        return;
    }

    /* Free the pbuf associated with this packet descriptor */
    if(pktDescPtr->pAppData != NULL)
    {
        pbuf_free(pktDescPtr->pAppData);
        pktDescPtr->pAppData = NULL;
    }

    if(pktDescPtr == &f2838xif_TxBringupDesc)
    {
        pktDescPtr->nextPacketDesc = NULL;
        f2838xif_TxBringupDescInUse = 0U;
        TxPktFreeQ.count = 1U;
    }
}

/**
 * This function should do the actual transmission of the packet. The packet is
 * contained in the pbuf that is passed to the function. This pbuf might be
 * chained.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @param p the MAC packet to send (e.g. IP packet including MAC addresses and type)
 * @return ERR_OK if the packet could be sent
 *         an err_t value if the packet couldn't be sent
 * @note This function MUST be called with interrupts disabled or with the
 *       F2838x Ethernet transmit fifo protected.
 */

static err_t
f2838xif_transmit(struct netif *netif, struct pbuf *p)
{
    struct pbuf *q;

    /* No of pbufs (if chained)*/
    int n=0;
    int i=0;
    Ethernet_Pkt_Desc *pktDescOrigPtr=NULL, *pktDescPtr=NULL, *lastPktDescPtr=NULL;

    /*
     * Make sure we still have a valid buffer (it may have been copied)
     */
    g_f2838xif_tx_stage = 1U;

    if(!p)
    {
        LINK_STATS_INC(link.memerr);
        ++g_f2838xif_tx_null_pbuf_count;
        g_f2838xif_tx_last_error = (uint32_t)ERR_MEM;
        g_f2838xif_tx_stage = 101U;

        return(ERR_MEM);
    }

    /*
     * Count the number of the pbufs in the chain that we are passed with.
     */
    for(q = p; q != NULL; q = q->next)
            n++;
    g_f2838xif_tx_last_chain_len = (uint32_t)n;
    g_f2838xif_tx_freeq_count =
        (f2838xif_TxBringupDescInUse == 0U) ? 1U : 0U;

    if((n != 1) || (f2838xif_TxBringupDescInUse != 0U))
    {
        LINK_STATS_INC(link.memerr);
        ++g_f2838xif_tx_no_desc_count;
        g_f2838xif_tx_last_error = (uint32_t)ERR_MEM;
        g_f2838xif_tx_stage = 102U;
        return (ERR_MEM);
    }

    DRIVER_STATS_ADD(TXCount, n);
    g_f2838xif_tx_stage = 2U;
    
    q = p; // Reset q to the head of the pbuf chain
    g_f2838xif_tx_stage = 21U;

    /* ENTER CRITICAL SECTION
     * This is to protect the forming of packetc descriptor chain using pbufs
     * passed to the function.
     */

    g_f2838xif_tx_stage = 22U;
    /* In the NO_SYS path this transmit can run from the Ethernet RX ISR
     * callback while replying to ICMP. Disabling core interrupts here can
     * prevent the path from returning on CM, so keep this critical section
     * open for bring-up and rely on the single-threaded ISR/main context.
     */
    g_f2838xif_tx_stage = 23U;

    while(i < n && q != NULL)
    {
        uint32_t flags = 0;
        g_f2838xif_tx_stage = 30U;
        if(i == 0)
        {
            flags |= ETHERNET_PKT_FLAG_SOP; // Start of packet
        }
            
        if(i == n-1)
        {
            flags |= ETHERNET_PKT_FLAG_EOP; // End of packet
        }
            

        Ethernet_Pkt_Desc *pktDescPtr = f2838xif_popTxBringupDesc();
        g_f2838xif_tx_stage = 31U;
        if(pktDescPtr == NULL)
        {
            /* EXIT CRITICAL SECTION */
            DRIVER_STATS_ADD(TXCopyFailCount, n);
            LINK_STATS_INC(link.memerr);
            ++g_f2838xif_tx_pop_null_count;
            g_f2838xif_tx_pop_null_freeq_count = TxPktFreeQ.count;
            g_f2838xif_tx_last_error = (uint32_t)ERR_MEM;
            g_f2838xif_tx_stage = 103U;
            return(ERR_MEM);
        }

        DRIVER_STATS_INC(TXNoDescCount);
        g_f2838xif_tx_stage = 32U;

        /* Initialize only the descriptor fields used by this Tx path. */
        g_f2838xif_tx_stage = 321U;
        pktDescPtr->nextPacketDesc = NULL;
        pktDescPtr->dataBuffer = NULL;
        pktDescPtr->pAppData = NULL;
        pktDescPtr->dataBuffer2 = NULL;
        g_f2838xif_tx_stage = 322U;
        pktDescPtr->bufferLength = 0U;
        pktDescPtr->buffer2Length = 0U;
        pktDescPtr->pktLength = 0U;
        pktDescPtr->validLength = 0U;
        g_f2838xif_tx_stage = 323U;
        pktDescPtr->dataOffset = 0U;
        pktDescPtr->flags = 0U;
        pktDescPtr->numPktFrags = 0U;
        pktDescPtr->timeStampLow = 0U;
        pktDescPtr->timeStampHigh = 0U;
        pktDescPtr->nextBufferDiscarded = 0U;
        pktDescPtr->extendedFlags = 0U;
        pktDescPtr->innerVlanTag = 0U;
        pktDescPtr->mssTso = 0U;
        pktDescPtr->vlanTag = 0U;
        g_f2838xif_tx_stage = 33U;

        if(q->len > sizeof(f2838xif_TxBringupBuffer))
        {
            LINK_STATS_INC(link.memerr);
            f2838xif_freePktDesc(pktDescPtr);
            ++g_f2838xif_tx_no_desc_count;
            g_f2838xif_tx_last_error = (uint32_t)ERR_MEM;
            g_f2838xif_tx_stage = 105U;
            return(ERR_MEM);
        }

        memcpy(f2838xif_TxBringupBuffer, q->payload, q->len);
        g_f2838xif_tx_stage = 330U;

        pktDescPtr->dataOffset = 0U;
        g_f2838xif_tx_stage = 331U;
        pktDescPtr->dataBuffer = f2838xif_TxBringupBuffer;
        g_f2838xif_tx_stage = 332U;
        pktDescPtr->pktChannel = ETHERNET_DMA_CHANNEL_NUM_0;
        g_f2838xif_tx_stage = 333U;
        pktDescPtr->pktLength = q->tot_len;
        g_f2838xif_tx_stage = 334U;
        pktDescPtr->bufferLength = q->len;
        g_f2838xif_tx_stage = 335U;
        pktDescPtr->validLength = q->len;
        g_f2838xif_tx_stage = 336U;
        pktDescPtr->flags = flags;
        g_f2838xif_tx_stage = 337U;
        pktDescPtr->nextPacketDesc = NULL;
        g_f2838xif_tx_stage = 338U;
        pktDescPtr->pAppData = NULL;
        g_f2838xif_tx_stage = 339U;
        g_f2838xif_tx_stage = 34U;

        if(i == 0)
        {
            pktDescOrigPtr = pktDescPtr; // Save the head of the chain
        }else
        {
            lastPktDescPtr->nextPacketDesc = pktDescPtr; // Chain the packet descriptors
        }
         
        lastPktDescPtr = pktDescPtr; // Save the last packet descriptor

        q = q->next; // Move to the next pbuf in the chain
        i++;
        g_f2838xif_tx_stage = 35U;
    }
    pktDescOrigPtr->numPktFrags = n;
    g_f2838xif_tx_stage = 3U;

#ifdef COE_ENABLE
    pktDescOrigPtr->flags |= ETHERNET_PKT_FLAG_CIC;
#endif

    DRIVER_STATS_ADD(TXCopyCount, n);

    ++g_f2838xif_transmit_count;
    g_f2838xif_tx_stage = 4U;
    if(Ethernet_sendPacket(emac_handle,pktDescOrigPtr) != ETHERNET_RET_SUCCESS)
    {
        if(g_f2838xif_tx_pending_eth_type == 0x0800U)
        {
            ++g_f2838xif_tx_ipv4_send_fail_count;
        }
        pktDescPtr = pktDescOrigPtr;
        while(pktDescPtr != NULL)
        {
            DRIVER_STATS_DEC(TXNoDescCount);
            pktDescOrigPtr = pktDescOrigPtr->nextPacketDesc;
            f2838xif_freePktDesc(pktDescPtr);
            pktDescPtr = pktDescOrigPtr;
        }
        LINK_STATS_INC(link.memerr);
        ++g_f2838xif_transmit_fail_count;
        g_f2838xif_tx_last_error = (uint32_t)ERR_MEM;
        g_f2838xif_tx_stage = 104U;
        return(ERR_MEM);
    }

    DRIVER_STATS_ADD(TXBufQueuedCount, n);
    ++g_f2838xif_transmit_ok_count;
    if(g_f2838xif_tx_pending_eth_type == 0x0800U)
    {
        ++g_f2838xif_tx_ipv4_send_ok_count;
    }
    g_f2838xif_tx_stage = 5U;

    LINK_STATS_INC(link.xmit);
    g_f2838xif_tx_last_error = (uint32_t)ERR_OK;

    return(ERR_OK);
}

/**
 * This function will process all transmit descriptors and free pbufs attached
 * to any that have been transmitted since we last checked.
 *
 * This function is called only from the Ethernet interrupt handler.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return None.
 */

static void
f2838xif_process_transmit(struct netif *netif, Ethernet_Pkt_Desc *pPacket)
{
    Ethernet_Pkt_Desc *pktDescPtr, *pktDescPtrShadow;

    /*
     * Free the packet descriptor memory.
     */
    if (pPacket == 0)
        return;

    pktDescPtr = pPacket;


    DRIVER_STATS_INC(TXBufFreedCount);
    f2838xif_freePktDesc(pktDescPtr);
    
}

void f2838xif_serviceTxQueue(void)
{
    Ethernet_Pkt_Desc *pktDescPtr;

    if(f2838xif_TxBringupDescInUse != 0U)
    {
        ++f2838xif_TxBringupDescInUseTicks;
        g_f2838xif_tx_inuse_spin_count = f2838xif_TxBringupDescInUseTicks;
        return;
    }

    if(f2838xif_TxPending == 0U)
    {
        return;
    }

    pktDescPtr = f2838xif_popTxBringupDesc();
    if(pktDescPtr == NULL)
    {
        ++g_f2838xif_tx_pop_null_count;
        g_f2838xif_tx_stage = 103U;
        return;
    }

    g_f2838xif_tx_stage = 401U;
    pktDescPtr->nextPacketDesc = NULL;
    pktDescPtr->dataBuffer = f2838xif_TxBringupBuffer;
    pktDescPtr->pAppData = NULL;
    pktDescPtr->dataBuffer2 = NULL;
    pktDescPtr->bufferLength = f2838xif_TxPendingLen;
    pktDescPtr->buffer2Length = 0U;
    pktDescPtr->pktChannel = ETHERNET_DMA_CHANNEL_NUM_0;
    pktDescPtr->pktLength = f2838xif_TxPendingLen;
    pktDescPtr->flags = ETHERNET_PKT_FLAG_SOP | ETHERNET_PKT_FLAG_EOP;
    pktDescPtr->numPktFrags = 1U;
    pktDescPtr->dataOffset = 0U;
    pktDescPtr->validLength = f2838xif_TxPendingLen;
    pktDescPtr->timeStampLow = 0U;
    pktDescPtr->timeStampHigh = 0U;
    pktDescPtr->nextBufferDiscarded = 0U;
    pktDescPtr->extendedFlags = 0U;
    pktDescPtr->innerVlanTag = 0U;
    pktDescPtr->mssTso = 0U;
    pktDescPtr->vlanTag = 0U;

    ++g_f2838xif_transmit_count;
    f2838xif_TxPending = 0U;
    f2838xif_TxBringupDescInUseTicks = 0U;
    g_f2838xif_tx_stage = 4U;
    if(Ethernet_sendPacket(emac_handle, pktDescPtr) != ETHERNET_RET_SUCCESS)
    {
        f2838xif_freePktDesc(pktDescPtr);
        ++g_f2838xif_transmit_fail_count;
        if(g_f2838xif_tx_pending_eth_type == 0x0800U)
        {
            ++g_f2838xif_tx_ipv4_send_fail_count;
        }
        g_f2838xif_tx_last_error = (uint32_t)ERR_MEM;
        g_f2838xif_tx_stage = 104U;
        return;
    }

    ++g_f2838xif_transmit_ok_count;
    f2838xif_TxBringupDescInUse = 0U;
    f2838xif_TxBringupDescInUseTicks = 0U;
    TxPktFreeQ.count = 1U;
    if(g_f2838xif_tx_pending_eth_type == 0x0800U)
    {
        ++g_f2838xif_tx_ipv4_send_ok_count;
    }
    g_f2838xif_tx_last_error = (uint32_t)ERR_OK;
    g_f2838xif_tx_stage = 5U;
    LINK_STATS_INC(link.xmit);
}

void f2838xif_noteTxComplete(void)
{
    f2838xif_TxBringupDescInUse = 0U;
    f2838xif_TxBringupDescInUseTicks = 0U;
    TxPktFreeQ.count = 1U;
    g_f2838xif_tx_stage = 6U;
}

/**
 * This function with either place the packet into the F2838x transmit fifo,
 * or will place the packet in the interface PBUF Queue for subsequent
 * transmission when the transmitter becomes idle.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @param p the MAC packet to send (e.g. IP packet including MAC addresses and type)
 * @return ERR_OK if the packet could be sent
 *         an err_t value if the packet couldn't be sent
 *
 */

static err_t
f2838xif_output(struct netif *netif, struct pbuf *p)
{
    struct pbuf *q;
    uint32_t copied = 0U;

    ++g_f2838xif_linkoutput_count;
    f2838xif_capture_tx_pbuf_info(p);

    if((p == NULL) || (p->tot_len > sizeof(f2838xif_TxBringupBuffer)) ||
       (f2838xif_TxPending != 0U))
    {
        ++g_f2838xif_linkoutput_error_count;
        g_f2838xif_tx_last_error = (uint32_t)ERR_MEM;
        g_f2838xif_tx_stage = 201U;
        return ERR_MEM;
    }

    g_f2838xif_tx_last_chain_len = 0U;
    for(q = p; q != NULL; q = q->next)
    {
        if((copied + q->len) > sizeof(f2838xif_TxBringupBuffer))
        {
            ++g_f2838xif_linkoutput_error_count;
            g_f2838xif_tx_last_error = (uint32_t)ERR_MEM;
            g_f2838xif_tx_stage = 202U;
            return ERR_MEM;
        }
        memcpy(&f2838xif_TxBringupBuffer[copied], q->payload, q->len);
        copied += q->len;
        ++g_f2838xif_tx_last_chain_len;
    }

    f2838xif_TxPendingLen = copied;
    f2838xif_TxPending = 1U;
    g_f2838xif_tx_freeq_count =
        (f2838xif_TxBringupDescInUse == 0U) ? 1U : 0U;
    g_f2838xif_tx_stage = 200U;

    return ERR_OK;
}

/**
 * This function will read a single packet from the F2838x ethernet
 * interface, if available, and return a pointer to a pbuf.  The timestamp
 * of the packet will be placed into the pbuf structure.
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return pointer to pbuf packet if available, NULL otherswise.
 */
Ethernet_Pkt_Desc*
f2838xif_receive( struct netif *netif, Ethernet_Pkt_Desc *pPacket )
{
    Ethernet_Pkt_Desc* newPktPtr;
    struct pbuf* p = pPacket->pAppData;
    uint32_t handledByBringup;

    if(p != NULL)
    {
        p->len = pPacket->validLength;
        p->tot_len = p->len;
        f2838xif_capture_rx_pbuf_info(p);

        /*
         * Keep ARP/ICMP raw replies as the Ethernet bring-up baseline.
         * TCP/HTTP raw handling is temporary and can be disabled once lwIP
         * owns the HTTP path.
         */
        handledByBringup =
            (f2838xif_try_raw_arp_reply(netif, p) != 0U) ||
            (f2838xif_try_raw_icmp_reply(netif, p) != 0U);
#if CM_ETH_BRINGUP_HTTP
        handledByBringup |=
            (f2838xif_is_raw_tcp_candidate(p) != 0U) &&
            (f2838xif_try_raw_tcp_http(netif, p) != 0U);
#endif
        if(handledByBringup != 0U)
        {
            LINK_STATS_INC(link.recv);
            return pPacket;
        }
    }

    newPktPtr = f2838xif_getFreeRxPacket();

    if(newPktPtr == NULL)
    {
        //Dont bother to pass to the lwip stack, Refill the buffer with existing packet;
        return pPacket;
    }

    pPacket->pAppData = NULL;

#if LWIP_PTPD
    u32_t time_s, time_ns;
    /* Get the current timestamp if PTPD is enabled */
    lwIPHostGetTime(&time_s, &time_ns);
#endif
    if(p)
    {
        err_t inputErr;
        p->len = pPacket->validLength;
        p->tot_len = p->len;
        f2838xif_capture_rx_pbuf_info(p);

    #if LWIP_PTPD
        /* Place the timestamp in the PBUF */
        p->time_s = time_s;
        p->time_ns = time_ns;
    #endif

        inputErr = netif->input(p, netif);
        g_f2838xif_last_input_error = (uint32_t)inputErr;
        if(inputErr != ERR_OK)
        {
            ++g_f2838xif_input_error_count;
            /* drop the packet */
            LWIP_DEBUGF(NETIF_DEBUG, ("f2838xif_input: input error\n"));
            pbuf_free(p);
            p = NULL;
            Ethernet_performPushOnPacketQueue(&RxPktFreeQ, pPacket);

            DRIVER_STATS_INC(RXPacketPostFailedCount);
            DRIVER_STATS_INC(RXPacketFreedCount);

            /* Adjust the link statistics */
            LINK_STATS_INC(link.memerr);
            LINK_STATS_INC(link.drop);
            return newPktPtr;
        }
        ++g_f2838xif_input_ok_count;
    }

    Ethernet_performPushOnPacketQueue(&RxPktFreeQ, pPacket);

    DRIVER_STATS_INC(RXPacketFreedCount);

    LINK_STATS_INC(link.recv);

    return(newPktPtr);
}


/**
 * Should be called at the beginning of the program to set up the
 * network interface. It calls the function f2838xif_hwinit() to do the
 * actual setup of the hardware.
 * This function should be passed as a parameter to netif_add().
 *
 * @param netif the lwip network interface structure for this ethernetif
 * @return ERR_OK if the loopif is initialized
 *         ERR_MEM if private data couldn't be allocated
 *         any other err_t on error
 */
err_t
f2838xif_init(struct netif *netif)
{
    LWIP_ASSERT("netif != NULL", (netif != NULL));

#if LWIP_NETIF_HOSTNAME
    /* Initialize interface hostname */
    netif->hostname = "lwip";
#endif /* LWIP_NETIF_HOSTNAME */

    /*
     * Initialize the snmp variables and counters inside the struct netif.
     * The last argument should be replaced with your link speed, in units
     * of bits per second.
     */
    NETIF_INIT_SNMP(netif, snmp_ifType_ethernet_csmacd, 1000000);

    netif->state = &f2838xif_data;
    netif->name[0] = IFNAME0;
    netif->name[1] = IFNAME1;

    /* We directly use etharp_output() here to save a function call.
     * You can instead declare your own function an call etharp_output()
     * from it if you have to do some checks before sending (e.g. if link
     * is available...) */
    netif->output = etharp_output;
    netif->linkoutput = f2838xif_output;

    f2838xif_data.ethaddr = (struct eth_addr *)&(netif->hwaddr[0]);


    /* initialize the hardware */
    f2838xif_hwinit(netif);

    return ERR_OK;
}

/**
 * Process tx and rx packets at the low-level interrupt.
 *
 * Should be called from the F2838x Ethernet Interrupt Handler.  This
 * function will read packets from the F2838x Ethernet fifo and place them
 * into a pbuf queue.  If the transmitter is idle and there is at least one packet
 * on the transmit queue, it will place it in the transmit fifo and start the
 * transmitter.
 *
 */

Ethernet_Pkt_Desc*
f2838xif_interrupt(struct netif *netif, Ethernet_Pkt_Desc *pPacket)
{
    Ethernet_Pkt_Desc *sPacket = NULL;
    /**
     * Based on the flags we get from pPacket, we should decide whether
     * to trasnmit or receive. Currently, it works for only incoming
     * ICMP ping requests.
     */

    /* ENTER CRITICAL SECTION
     * This is to protect the forming of packetc descriptor chain using pbufs
     * passed to the function.
     */

    //Ethernet_device_struct.ptrCoreInterruptDisable();

    if(pPacket->flags & ETHERNET_INTERRUPT_FLAG_RECEIVE)    
        sPacket = f2838xif_receive(netif, pPacket);

    if(pPacket->flags & ETHERNET_INTERRUPT_FLAG_TRANSMIT)
        f2838xif_process_transmit(netif, pPacket);

    //Ethernet_device_struct.ptrCoreInterruptEnable();

    return (sPacket);
}

/*
f2838xif_freePbufs

This functions frees every pbufs allocated to Rx Packet descriptors
*/
static void f2838xif_freePbufs(void)
{
    uint32_t index;
    for(index = 0; index < NUM_PACKET_DESC_RX_APPLICATION; index++)
    {
        Ethernet_Pkt_Desc* pPacket = &f2838xif_PktDesc[index];

        struct pbuf* p = (struct pbuf*)pPacket->pAppData;
        if(p)
        {
            pbuf_free(p);
            pPacket->pAppData = NULL;
        }
    }
}

/*
f2838xif_getFreeRxPacket

Function will pop out and return descriptors from Rx Free Queue and allocate a pbuf along with it.
It could get called by f2838xif_receive function for repopulating the buffers.
It could also get called from Application layer as call back function for getting free Rx Descriptors.

Note :- Make sure following functions are called before this function gets called.
            ->lwip_init();
            ->f2838xif_getFreeRxPacket.
          Ethernet_init() function calls this function as a callback.
*/
Ethernet_Pkt_Desc* f2838xif_getFreeRxPacket(void)
{
    //
    // First check if it is called by channel Init Stage
    // If so first call f2838xif_freePbufs inorder to free
    // any buffer hold by the Rx descriptors.
    //
    Ethernet_RxChDesc* channelDescPtr = &(Ethernet_device_struct.dmaObj.rxDma[ETHERNET_DMA_CHANNEL_NUM_0]);
    if(Ethernet_HW_descQueueGetCount(channelDescPtr) == 0)
    {   
        f2838xif_freePbufs();

        //
        // Re-initialize RxPktFreeQ.
        //
        RxPktFreeQ.head = RxPktFreeQ.tail = NULL;
        RxPktFreeQ.count = 0U;

        uint32_t index;
        for(index = 0; index < NUM_PACKET_DESC_RX_APPLICATION; index++)
        {
            Ethernet_performPushOnPacketQueue(&RxPktFreeQ, &f2838xif_PktDesc[index]);
        }
    }

    Ethernet_Pkt_Desc* pPacket = Ethernet_performPopOnPacketQueue(&RxPktFreeQ);
    if(pPacket == NULL)
    {
        DRIVER_STATS_INC(RXPacketErrCount);
        return NULL;
    }

    DRIVER_STATS_INC(RXPacketReadCount);

    struct pbuf* p = pbuf_alloc(PBUF_RAW, 1538 , PBUF_POOL);
    if(p == NULL)
    {
        DRIVER_STATS_INC(RXPbufAllocFailCount);
        Ethernet_performPushOnPacketQueue(&RxPktFreeQ, pPacket);
        return NULL;
    }

    DRIVER_STATS_INC(RXBufReadCount);

    pPacket->dataOffset = 0;
    pPacket->dataBuffer = p->payload;
    pPacket->pAppData = p;

    return pPacket;
}

/*
f283xif_initQueue

Rx and Tx Free Queues are getting initialized
Rx and Tx Free Queues should get initialized before any Rx or Tx operation starts.
It should get called before f2838xif_initQueue gets called.
*/

void f2838xif_initQueue(void)
{
    uint32_t index;

    //initialize the Rx Free Queue
    RxPktFreeQ.head = &f2838xif_PktDesc[0];

    for(index = 1; index < NUM_PACKET_DESC_RX_APPLICATION ; index++)
    {
        f2838xif_PktDesc[index - 1].nextPacketDesc = &f2838xif_PktDesc[index];
    }

    RxPktFreeQ.tail = &f2838xif_PktDesc[NUM_PACKET_DESC_RX_APPLICATION - 1];
    RxPktFreeQ.count = NUM_PACKET_DESC_RX_APPLICATION;


    //initialize the Tx Free Queue
    TxPktFreeQ.head = NULL;
    TxPktFreeQ.tail = NULL;
    TxPktFreeQ.count = 1U;
    f2838xif_TxBringupDescInUse = 0U;
    f2838xif_TxBringupDescInUseTicks = 0U;
    f2838xif_TxPending = 0U;
    f2838xif_TxPendingLen = 0U;
    f2838xif_TxBringupDesc.nextPacketDesc = NULL;
    f2838xif_TxBringupDesc.pAppData = NULL;

}
