#include "../CM/include_editable/initial_header.h"
#include "../CM/include_editable/comm_cm.h"
#include "inc/hw_emac_ss.h"
#include "inc/hw_usb.h"

extern "C" void Ethernet_resetModule(uint32_t baseAddress);

#define CM_TEST_ETHERNET_MDIO_ENABLE 1U
#define CM_TEST_USB_REGISTER_ENABLE 1U
#define CPU1_TO_CM_BOOT_READY_TOKEN 0xC001CAFEUL
#define CM_TO_CPU1_ACK_READY_TOKEN 0xACCECA11UL
#define CM_HANDSHAKE_IDLE_DELAY_US 1000U
#define CM_UART_TX_BAUDRATE 115200U
#define CM_UART_TX_IDLE_DELAY_US 100000U
#define CM_ETHERNET_PHY_ADDR_COUNT 32U
#define CM_ETHERNET_PHY_REG_BMSR 1U
#define CM_ETHERNET_PHY_REG_ID1 2U
#define CM_ETHERNET_PHY_REG_ID2 3U
#define CM_ETHERNET_MDIO_POLL_DELAY_US 100000U
#define CM_USB_REGISTER_POLL_DELAY_US 100000U
#define CM_LIVENESS_HEARTBEAT_START 1U
#define CM_ETHERNET_PHY_BMSR_LINK_STATUS 0x0004U
#define CM_ETHERNET_PHY_LOST_LIMIT 10U

// MSGRAM mailbox는 양쪽 코어가 같은 주소를 보도록 section 시작(offset 0)에 둔다.
// CPU1 쪽(intercore_cpu1.c)도 pad 없이 offset 0이므로 driverlib buffer pad를 두지
// 않는다. raw mailbox polling만 쓰므로 pad가 필요 없다.
#pragma DATA_SECTION("MSGRAM_CPU1_TO_CM")
volatile uint32_t g_cpu1_to_cm_mailbox;

#pragma DATA_SECTION("MSGRAM_CM_TO_CPU1")
volatile uint32_t g_cm_to_cpu1_mailbox;

volatile uint32_t g_cm_main_entered = 0U;
volatile uint32_t g_cm_local_handshake_status = 0U;
volatile uint32_t g_cm_uart_tx_count = 0U;
volatile uint32_t g_cm_ethernet_init_stage = 0U;
volatile uint32_t g_cm_ethernet_phy_scan_count = 0U;
volatile uint32_t g_cm_ethernet_phy_found_mask = 0U;
volatile uint32_t g_cm_ethernet_phy_active_addr = 0xffffffffUL;
volatile uint32_t g_cm_ethernet_phy_bmsr = 0U;
volatile uint32_t g_cm_ethernet_phy_id1 = 0U;
volatile uint32_t g_cm_ethernet_phy_id2 = 0U;
volatile uint32_t g_cm_ethernet_ss_ctrlsts = 0U;
volatile uint32_t g_cm_ethernet_mdio_address = 0U;
volatile uint32_t g_cm_ethernet_mdio_data = 0U;
volatile uint16_t g_cm_ethernet_phy_bmsr_by_addr[CM_ETHERNET_PHY_ADDR_COUNT];
volatile uint16_t g_cm_ethernet_phy_id1_by_addr[CM_ETHERNET_PHY_ADDR_COUNT];
volatile uint16_t g_cm_ethernet_phy_id2_by_addr[CM_ETHERNET_PHY_ADDR_COUNT];
volatile uint32_t g_cm_ethernet_link_up = 0U;
volatile uint32_t g_cm_ethernet_link_up_count = 0U;
volatile uint32_t g_cm_ethernet_link_down_count = 0U;
volatile uint32_t g_cm_ethernet_phy_lost_count = 0U;
volatile uint32_t g_cm_ethernet_recovery_count = 0U;
volatile uint32_t g_cm_ethernet_recovery_stage = 0U;
volatile uint32_t g_cm_usb_poll_count = 0U;
volatile uint32_t g_cm_usb_devctl = 0U;
volatile uint32_t g_cm_usb_power = 0U;
volatile uint32_t g_cm_usb_int_status = 0U;
volatile uint32_t g_cm_liveness_count = 0U;

static void comm_cm_uart_write_string(const char *text)
{
    while (*text != '\0')
    {
        UART_writeChar(UART0_BASE, (uint8_t)*text);
        ++text;
    }
}

static void cm_ethernet_select_mii_mode(void)
{
    HWREG(EMAC_SS_BASE + ETHERNETSS_O_CTRLSTS) =
        (((uint32_t)ETHERNET_SS_CTRLSTS_WRITE_KEY_VALUE <<
          ETHERNETSS_CTRLSTS_WRITE_KEY_S) &
         ETHERNETSS_CTRLSTS_WRITE_KEY_M) |
        ((uint32_t)ETHERNET_SS_PHY_INTF_SEL_MII <<
         ETHERNETSS_CTRLSTS_PHY_INTF_SEL_S);
}

static void cm_ethernet_select_phy_addr(uint32_t addr)
{
    HWREG(EMAC_BASE + ETHERNET_O_MAC_MDIO_ADDRESS) =
        (5UL << ETHERNET_MAC_MDIO_ADDRESS_CR_S) |
        ((addr & 0x1fUL) << ETHERNET_MAC_MDIO_ADDRESS_PA_S);
}

void comm_cm_init_status(void)
{
    g_cm_main_entered = 1U;
    g_cm_local_handshake_status = CM_STATUS_WAITING_BOOT_READY;
}

void comm_cm_wait_for_cpu1_boot_ready_and_ack(void)
{
    while (g_cpu1_to_cm_mailbox != CPU1_TO_CM_BOOT_READY_TOKEN)
    {
        DEVICE_DELAY_US(CM_HANDSHAKE_IDLE_DELAY_US);
    }

    g_cm_to_cpu1_mailbox = CM_TO_CPU1_ACK_READY_TOKEN;
    g_cm_local_handshake_status = CM_STATUS_ACK_WRITTEN;
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
    g_cm_ethernet_init_stage = 1U;
    Platform_enablePeripheral(SYSCTL_PERIPH_CLK_ENET);
    g_cm_ethernet_init_stage = 2U;
    cm_ethernet_select_mii_mode();
    g_cm_ethernet_init_stage = 3U;
    SysCtl_resetPeripheral((SysCtl_PeripheralSOFTPRES)SYSCTL_PERIPH_CLK_ENET);
    g_cm_ethernet_init_stage = 4U;
    Ethernet_resetModule(EMAC_BASE);
    g_cm_ethernet_init_stage = 5U;
    Ethernet_configureMDIO(EMAC_BASE, 0U, 5U, 0U);
    g_cm_ethernet_ss_ctrlsts = HWREG(EMAC_SS_BASE + ETHERNETSS_O_CTRLSTS);
    g_cm_ethernet_mdio_address =
        HWREG(EMAC_BASE + ETHERNET_O_MAC_MDIO_ADDRESS);
    g_cm_ethernet_mdio_data = HWREG(EMAC_BASE + ETHERNET_O_MAC_MDIO_DATA);
    g_cm_ethernet_init_stage = 6U;
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
        cm_ethernet_select_phy_addr(addr);
        g_cm_ethernet_mdio_address =
            HWREG(EMAC_BASE + ETHERNET_O_MAC_MDIO_ADDRESS);

        bmsr = Ethernet_readPHYRegister(EMAC_BASE, CM_ETHERNET_PHY_REG_BMSR);
        id1 = Ethernet_readPHYRegister(EMAC_BASE, CM_ETHERNET_PHY_REG_ID1);
        id2 = Ethernet_readPHYRegister(EMAC_BASE, CM_ETHERNET_PHY_REG_ID2);
        g_cm_ethernet_mdio_address =
            HWREG(EMAC_BASE + ETHERNET_O_MAC_MDIO_ADDRESS);
        g_cm_ethernet_mdio_data = HWREG(EMAC_BASE + ETHERNET_O_MAC_MDIO_DATA);

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

    if (foundMask != 0U)
    {
        //
        // PHY가 보이면 MDIO/MAC 경로는 정상이다. BMSR link bit으로 링크 상태만
        // 추적한다. cable이 빠진 단순 link-down은 PHY가 스스로 재링크하므로
        // MAC 재초기화(recovery)는 하지 않는다.
        //
        g_cm_ethernet_phy_lost_count = 0U;

        if ((g_cm_ethernet_phy_bmsr & CM_ETHERNET_PHY_BMSR_LINK_STATUS) != 0U)
        {
            if (g_cm_ethernet_link_up == 0U)
            {
                ++g_cm_ethernet_link_up_count;
            }
            g_cm_ethernet_link_up = 1U;
        }
        else
        {
            if (g_cm_ethernet_link_up != 0U)
            {
                ++g_cm_ethernet_link_down_count;
            }
            g_cm_ethernet_link_up = 0U;
        }
    }
    else
    {
        //
        // PHY 자체가 안 보임 = MDIO/MAC/clock 경로 이상. 이 상태가
        // CM_ETHERNET_PHY_LOST_LIMIT회 연속 지속되면 MAC/MDIO를 재초기화해 복구한다.
        //
        if (g_cm_ethernet_link_up != 0U)
        {
            ++g_cm_ethernet_link_down_count;
        }
        g_cm_ethernet_link_up = 0U;
        g_cm_ethernet_phy_active_addr = 0xffffffffUL;

        if (g_cm_ethernet_phy_lost_count < CM_ETHERNET_PHY_LOST_LIMIT)
        {
            ++g_cm_ethernet_phy_lost_count;
        }
        else
        {
            g_cm_ethernet_recovery_stage = 1U;
            comm_cm_ethernet_mdio_test_init();
            ++g_cm_ethernet_recovery_count;
            g_cm_ethernet_phy_lost_count = 0U;
            g_cm_ethernet_recovery_stage = 2U;
        }
    }

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

//
// Liveness heartbeat.
//
// Handshake가 끝난 뒤에는 CM->CPU1 mailbox를 정적 ACK token 대신 단조 증가하는
// heartbeat 값으로 재사용한다. CPU1 supervisor는 이 값이 변하는지로 CM 생존을
// 판단하므로, mailbox 값이 멈추면(루프 정지/코어 다운) recovery가 동작한다.
//
// 이 함수는 CM main loop의 맨 끝에서 호출한다. ACK write 직후가 아니라 한 루프
// (UART/Ethernet/USB 서비스 지연 합 ~300ms) 뒤에 첫 갱신이 일어나므로,
// CPU1 handshake polling이 ACK token을 latch하기 전에 덮어쓰는 race가 없다.
//
void comm_cm_liveness_service(void)
{
    if (g_cm_liveness_count < CM_LIVENESS_HEARTBEAT_START)
    {
        g_cm_liveness_count = CM_LIVENESS_HEARTBEAT_START;
    }
    else
    {
        ++g_cm_liveness_count;
    }

    g_cm_to_cpu1_mailbox = g_cm_liveness_count;
}
