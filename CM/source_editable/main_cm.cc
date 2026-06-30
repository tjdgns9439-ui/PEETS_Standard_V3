
//
// Included Files
//
#include "../CM/include_editable/initial_header.h"
#include "../CM/include_editable/comm_cm.h"

//
// Main
//
void main(void)
{
    CM_init();
    comm_cm_init_status();
    comm_cm_wait_for_cpu1_boot_ready_and_ack();
    comm_cm_uart_tx_init();
    comm_cm_ethernet_mdio_test_init();
    comm_cm_usb_register_test_init();

    for (;;)
    {
        comm_cm_uart_tx_service();
        comm_cm_ethernet_mdio_test_service();
        comm_cm_usb_register_test_service();
    }
}

//
// End of File
//
