#include "../CM/include_editable/initial_header.h"
#include "../CM/include_editable/comm_cm.h"
#include "inc/hw_usb.h"

#define CM_TEST_ETHERNET_MDIO_ENABLE 1U
#define CM_TEST_USB_REGISTER_ENABLE 1U
#define CPU1_TO_CM_BOOT_READY_TOKEN 0xC001CAFEUL
#define CM_TO_CPU1_ACK_READY_TOKEN 0xACCECA11UL
#define CM_HANDSHAKE_IDLE_DELAY_US 1000U
#define CPU1_CM_IPC_DRIVERLIB_BUFFER_WORDS 68U
#define CM_UART_TX_BAUDRATE 115200U
#define CM_UART_TX_IDLE_DELAY_US 100000U
#define CM_ETHERNET_PHY_ADDR_COUNT 32U
#define CM_ETHERNET_PHY_REG_BMSR 1U
#define CM_ETHERNET_PHY_REG_ID1 2U
#define CM_ETHERNET_PHY_REG_ID2 3U
#define CM_ETHERNET_MDIO_POLL_DELAY_US 100000U
#define CM_USB_REGISTER_POLL_DELAY_US 100000U

#pragma DATA_SECTION("MSGRAM_CPU1_TO_CM")
volatile uint32_t g_cpu1_to_cm_ipc_buffer_pad[CPU1_CM_IPC_DRIVERLIB_BUFFER_WORDS];

#pragma DATA_SECTION("MSGRAM_CPU1_TO_CM")
volatile uint32_t g_cpu1_to_cm_mailbox;

#pragma DATA_SECTION("MSGRAM_CM_TO_CPU1")
volatile uint32_t g_cm_to_cpu1_ipc_buffer_pad[CPU1_CM_IPC_DRIVERLIB_BUFFER_WORDS];

#pragma DATA_SECTION("MSGRAM_CM_TO_CPU1")
volatile uint32_t g_cm_to_cpu1_mailbox;

volatile uint32_t g_cm_main_entered = 0U;
volatile uint32_t g_cm_handshake_status = 0U;
volatile uint32_t g_cm_uart_tx_count = 0U;
volatile uint32_t g_cm_ethernet_phy_scan_count = 0U;
volatile uint32_t g_cm_ethernet_phy_found_mask = 0U;
volatile uint32_t g_cm_ethernet_phy_active_addr = 0xffffffffUL;
volatile uint32_t g_cm_ethernet_phy_bmsr = 0U;
volatile uint32_t g_cm_ethernet_phy_id1 = 0U;
volatile uint32_t g_cm_ethernet_phy_id2 = 0U;
volatile uint16_t g_cm_ethernet_phy_bmsr_by_addr[CM_ETHERNET_PHY_ADDR_COUNT];
volatile uint16_t g_cm_ethernet_phy_id1_by_addr[CM_ETHERNET_PHY_ADDR_COUNT];
volatile uint16_t g_cm_ethernet_phy_id2_by_addr[CM_ETHERNET_PHY_ADDR_COUNT];
volatile uint32_t g_cm_usb_poll_count = 0U;
volatile uint32_t g_cm_usb_devctl = 0U;
volatile uint32_t g_cm_usb_power = 0U;
volatile uint32_t g_cm_usb_int_status = 0U;

static void comm_cm_uart_write_string(const char *text)
{
    while (*text != '\0')
    {
        UART_writeChar(UART0_BASE, (uint8_t)*text);
        ++text;
    }
}

void comm_cm_init_status(void)
{
    g_cm_main_entered = 1U;
    g_cm_handshake_status = CM_STATUS_WAITING_BOOT_READY;
}

void comm_cm_wait_for_cpu1_boot_ready_and_ack(void)
{
    while (g_cpu1_to_cm_mailbox != CPU1_TO_CM_BOOT_READY_TOKEN)
    {
        DEVICE_DELAY_US(CM_HANDSHAKE_IDLE_DELAY_US);
    }

    g_cm_to_cpu1_mailbox = CM_TO_CPU1_ACK_READY_TOKEN;
    g_cm_handshake_status = CM_STATUS_ACK_WRITTEN;
}

void comm_cm_uart_tx_init(void)
{
    UART_setConfig(UART0_BASE,
                   UART_CLK_FREQ,
                   CM_UART_TX_BAUDRATE,
                   UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                       UART_CONFIG_PAR_NONE);
    HWREG(UART0_BASE + UART_O_CTL) &= ~UART_CTL_RXE;
}

void comm_cm_uart_tx_service(void)
{
    comm_cm_uart_write_string("CM UART TX\r\n");
    ++g_cm_uart_tx_count;
    DEVICE_DELAY_US(CM_UART_TX_IDLE_DELAY_US);
}

void comm_cm_ethernet_mdio_test_init(void)
{
#if CM_TEST_ETHERNET_MDIO_ENABLE
    Ethernet_configureMDIO(EMAC_BASE, 0U, 5U, 0U);
#endif
}

void comm_cm_ethernet_mdio_test_service(void)
{
#if CM_TEST_ETHERNET_MDIO_ENABLE
    uint32_t addr;
    uint16_t bmsr;
    uint16_t id1;
    uint16_t id2;
    uint32_t foundMask = 0U;

    for (addr = 0U; addr < CM_ETHERNET_PHY_ADDR_COUNT; ++addr)
    {
        Ethernet_configurePHYAddress(EMAC_BASE, (uint8_t)addr);

        bmsr = Ethernet_readPHYRegister(EMAC_BASE, CM_ETHERNET_PHY_REG_BMSR);
        id1 = Ethernet_readPHYRegister(EMAC_BASE, CM_ETHERNET_PHY_REG_ID1);
        id2 = Ethernet_readPHYRegister(EMAC_BASE, CM_ETHERNET_PHY_REG_ID2);

        g_cm_ethernet_phy_bmsr_by_addr[addr] = bmsr;
        g_cm_ethernet_phy_id1_by_addr[addr] = id1;
        g_cm_ethernet_phy_id2_by_addr[addr] = id2;

        if ((id1 != 0x0000U) && (id1 != 0xffffU) &&
            (id2 != 0x0000U) && (id2 != 0xffffU))
        {
            foundMask |= (1UL << addr);
            g_cm_ethernet_phy_active_addr = addr;
            g_cm_ethernet_phy_bmsr = bmsr;
            g_cm_ethernet_phy_id1 = id1;
            g_cm_ethernet_phy_id2 = id2;
            break;
        }
    }

    g_cm_ethernet_phy_found_mask = foundMask;
    ++g_cm_ethernet_phy_scan_count;
    DEVICE_DELAY_US(CM_ETHERNET_MDIO_POLL_DELAY_US);
#endif
}

void comm_cm_usb_register_test_init(void)
{
#if CM_TEST_USB_REGISTER_ENABLE
    USBDevDisconnect(USB0_BASE);
    USBIntDisableControl(USB0_BASE, USB_INTCTRL_ALL);
    USBIntDisableEndpoint(USB0_BASE, USB_INTEP_ALL);
#endif
}

void comm_cm_usb_register_test_service(void)
{
#if CM_TEST_USB_REGISTER_ENABLE
    uint32_t endpointStatus = 0U;

    g_cm_usb_devctl = HWREGB(USB0_BASE + USB_O_DEVCTL);
    g_cm_usb_power = HWREGB(USB0_BASE + USB_O_POWER);
    g_cm_usb_int_status = USBIntStatus(USB0_BASE, &endpointStatus);
    ++g_cm_usb_poll_count;
    DEVICE_DELAY_US(CM_USB_REGISTER_POLL_DELAY_US);
#endif
}
