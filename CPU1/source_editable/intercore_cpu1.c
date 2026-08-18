#include "../CPU1/include_editable/initial_header.h"
#include "../CPU1/include_editable/intercore_cpu1.h"
#include "../CPU1/include_editable/watchdog.h"

#define CM_HANDSHAKE_POLL_DELAY_US 1000U
#define CM_HANDSHAKE_TIMEOUT_POLLS 5000U
#define CPU2_HANDSHAKE_POLL_DELAY_US 1000U
#define CPU2_HANDSHAKE_TIMEOUT_POLLS 5000U

#define CPU1_TO_CM_BOOT_READY_TOKEN 0xC001CAFEUL
#define CM_TO_CPU1_ACK_READY_TOKEN 0xACCECA11UL
#define CPU1_TO_CPU2_BOOT_READY_TOKEN 0xC002CAFEUL
#define CPU2_TO_CPU1_ACK_READY_TOKEN 0xACCE2C11UL

volatile uint32_t g_cm_handshake_status = 0U;
volatile uint32_t g_cm_handshake_poll_count = 0U;
volatile uint32_t g_cpu2_handshake_status = 0U;
volatile uint32_t g_cpu2_handshake_poll_count = 0U;

#pragma DATA_SECTION(g_cpu1_to_cm_mailbox, "MSGRAM_CPU_TO_CM")
volatile uint32_t g_cpu1_to_cm_mailbox;

#pragma DATA_SECTION(g_cm_to_cpu1_mailbox, "MSGRAM_CM_TO_CPU")
volatile uint32_t g_cm_to_cpu1_mailbox;

// CPU1 -> CPU2 command block. mbox (field [0]) is the handshake token (same
// address as the old g_cpu1_to_cpu2_mailbox); the rest carries commands that
// CPU1/easyDSP write and CPU2 applies. App IPC section at the HIGH end of the
// CPU1->CPU2 MSGRAM (NOT the driverlib-shared section); sole object so CPU1 and
// CPU2 place it at the same address.
#pragma DATA_SECTION(g_cpu2_command, "MSGRAM_APP_C1TOC2")
volatile cpu2_command_t g_cpu2_command;

// CPU2 -> CPU1 MSGRAM: single monitor mirror struct. Its first word (mbox) is
// the handshake/liveness mailbox (same address as the old g_cpu2_to_cpu1_mailbox);
// the rest mirrors CPU2 state for easyDSP to read via CPU1. Sole object in this
// section so CPU1 and CPU2 place it at the same address (0x03B000).
#pragma DATA_SECTION(g_cpu2_monitor, "MSGRAM_APP_C2TOC1")
volatile cpu2_monitor_t g_cpu2_monitor;

static void intercore_cpu1_boot_cm(void)
{
#ifdef _FLASH
    Device_bootCM(BOOTMODE_BOOT_TO_FLASH_SECTOR0);
#else
    /*
     * RAM build: all cores are loaded and released by the JTAG debugger, so
     * CPU1 must NOT boot CM here. Device_bootCM() would hang in its IPC sync
     * waiting for CM's boot ROM, but CM is already executing the debugger-loaded
     * image (not in the boot ROM). The mailbox handshake below still runs.
     */
#endif
}

static void intercore_cpu1_boot_cpu2(void)
{
#ifdef _FLASH
    Device_bootCPU2(BOOTMODE_BOOT_TO_FLASH_SECTOR0);
#else
    /*
     * RAM build: CPU2 is loaded/released by the JTAG debugger; CPU1 must not
     * boot it (Device_bootCPU2 would hang in IPC sync). Handshake still runs.
     */
#endif
}

void intercore_cpu1_run_cm_handshake(void)
{
    uint32_t poll_count;

    g_cm_handshake_status = CM_HANDSHAKE_STATUS_IDLE;
    g_cm_handshake_poll_count = 0U;
    g_cpu1_to_cm_mailbox = 0U;
    g_cm_to_cpu1_mailbox = 0U;

    intercore_cpu1_boot_cm();
    g_cm_handshake_status = CM_HANDSHAKE_STATUS_BOOTED;

    g_cpu1_to_cm_mailbox = CPU1_TO_CM_BOOT_READY_TOKEN;
    g_cm_handshake_status = CM_HANDSHAKE_STATUS_WAITING_ACK;

    for (poll_count = 0U; poll_count < CM_HANDSHAKE_TIMEOUT_POLLS; ++poll_count)
    {
        if (g_cm_to_cpu1_mailbox == CM_TO_CPU1_ACK_READY_TOKEN)
        {
            g_cm_handshake_poll_count = poll_count;
            g_cm_handshake_status = CM_HANDSHAKE_STATUS_OK;
            return;
        }

        watchdog_service(); // keep the (up to 5 s) poll from tripping the WD
        DEVICE_DELAY_US(CM_HANDSHAKE_POLL_DELAY_US);
    }

    g_cm_handshake_poll_count = CM_HANDSHAKE_TIMEOUT_POLLS;
    g_cm_handshake_status = CM_HANDSHAKE_STATUS_TIMEOUT;
}

void intercore_cpu1_run_cpu2_handshake(void)
{
    uint32_t poll_count;

    g_cpu2_handshake_status = CPU2_HANDSHAKE_STATUS_IDLE;
    g_cpu2_handshake_poll_count = 0U;
    g_cpu2_command.mbox = 0U;
    // Clean command block before CPU2 boots so it never applies NOINIT garbage.
    g_cpu2_command.seq = 0U;
    g_cpu2_command.enable = 0U;
    g_cpu2_command.mode = 0U;
    g_cpu2_command.setpoint = 0U;
    g_cpu2_command.estop = 0U;
    g_cpu2_monitor.mbox = 0U;

    intercore_cpu1_boot_cpu2();
    g_cpu2_handshake_status = CPU2_HANDSHAKE_STATUS_BOOTED;

    g_cpu2_command.mbox = CPU1_TO_CPU2_BOOT_READY_TOKEN;
    g_cpu2_handshake_status = CPU2_HANDSHAKE_STATUS_WAITING_ACK;

    for (poll_count = 0U; poll_count < CPU2_HANDSHAKE_TIMEOUT_POLLS; ++poll_count)
    {
        if (g_cpu2_monitor.mbox == CPU2_TO_CPU1_ACK_READY_TOKEN)
        {
            g_cpu2_handshake_poll_count = poll_count;
            g_cpu2_handshake_status = CPU2_HANDSHAKE_STATUS_OK;
            return;
        }

        watchdog_service(); // keep the (up to 5 s) poll from tripping the WD
        DEVICE_DELAY_US(CPU2_HANDSHAKE_POLL_DELAY_US);
    }

    g_cpu2_handshake_poll_count = CPU2_HANDSHAKE_TIMEOUT_POLLS;
    g_cpu2_handshake_status = CPU2_HANDSHAKE_STATUS_TIMEOUT;
}
