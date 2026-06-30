#ifndef CM_INCLUDE_EDITABLE_COMM_CM_H_
#define CM_INCLUDE_EDITABLE_COMM_CM_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum
{
    CM_STATUS_IDLE = 0U,
    CM_STATUS_WAITING_BOOT_READY = 1U,
    CM_STATUS_ACK_WRITTEN = 2U
};

extern volatile uint32_t g_cm_main_entered;
extern volatile uint32_t g_cm_handshake_status;
extern volatile uint32_t g_cm_uart_tx_count;
extern volatile uint32_t g_cm_ethernet_phy_scan_count;
extern volatile uint32_t g_cm_ethernet_phy_found_mask;
extern volatile uint32_t g_cm_ethernet_phy_active_addr;
extern volatile uint32_t g_cm_ethernet_phy_bmsr;
extern volatile uint32_t g_cm_ethernet_phy_id1;
extern volatile uint32_t g_cm_ethernet_phy_id2;
extern volatile uint16_t g_cm_ethernet_phy_bmsr_by_addr[32];
extern volatile uint16_t g_cm_ethernet_phy_id1_by_addr[32];
extern volatile uint16_t g_cm_ethernet_phy_id2_by_addr[32];
extern volatile uint32_t g_cm_usb_poll_count;
extern volatile uint32_t g_cm_usb_devctl;
extern volatile uint32_t g_cm_usb_power;
extern volatile uint32_t g_cm_usb_int_status;
extern volatile uint32_t g_cpu1_to_cm_mailbox;
extern volatile uint32_t g_cm_to_cpu1_mailbox;

void comm_cm_init_status(void);
void comm_cm_wait_for_cpu1_boot_ready_and_ack(void);
void comm_cm_uart_tx_init(void);
void comm_cm_uart_tx_service(void);
void comm_cm_ethernet_mdio_test_init(void);
void comm_cm_ethernet_mdio_test_service(void);
void comm_cm_usb_register_test_init(void);
void comm_cm_usb_register_test_service(void);

#ifdef __cplusplus
}
#endif

#endif
