#include "../CPU2/include_editable/initial_header.h"
#include "../CPU2/include_editable/intercore_cpu2.h"

#define CPU1_TO_CPU2_BOOT_READY_TOKEN 0xC002CAFEUL
#define CPU2_TO_CPU1_ACK_READY_TOKEN 0xACCE2C11UL
#define CPU2_LIVENESS_HEARTBEAT_START 1U

volatile uint32_t g_cpu2_main_entered = 0U;
volatile uint32_t g_cpu2_tick_count = 0U;
volatile uint32_t g_cpu2_local_handshake_status = 0U;
volatile uint32_t g_cpu2_liveness_count = 0U;

// CPU1 -> CPU2 command block (see cpu2_monitor_shared.h). mbox (field [0]) is the
// handshake token (same address as the old g_cpu1_to_cpu2_mailbox); the rest are
// commands CPU1/easyDSP write and CPU2 applies. App IPC section at the HIGH end
// of the CPU1->CPU2 MSGRAM, matches CPU1's placement.
#pragma DATA_SECTION(g_cpu2_command, "MSGRAM_APP_C1TOC2")
volatile cpu2_command_t g_cpu2_command;

// CPU2 -> CPU1 MSGRAM: single monitor mirror struct (see cpu2_monitor_shared.h).
// mbox is the handshake/liveness mailbox (same address as the old
// g_cpu2_to_cpu1_mailbox); the other fields mirror CPU2 state so the single
// CPU1 SCI-A easyDSP module can read CPU2 without a 2nd SCI. Sole object in this
// section so CPU1 and CPU2 place it at the same address (0x03B000).
#pragma DATA_SECTION(g_cpu2_monitor, "MSGRAM_APP_C2TOC1")
volatile cpu2_monitor_t g_cpu2_monitor;

void intercore_cpu2_init(void)
{
    g_cpu2_main_entered = 1U;
    g_cpu2_local_handshake_status = CPU2_LOCAL_HANDSHAKE_STATUS_WAITING_TOKEN;

    // Seed the monitor mirror (MSGRAM is NOINIT). Do NOT touch mbox here: it is
    // driven by the boot handshake with CPU1.
    g_cpu2_monitor.main_entered = g_cpu2_main_entered;
    g_cpu2_monitor.tick_count = 0U;
    g_cpu2_monitor.local_handshake_status = g_cpu2_local_handshake_status;
    g_cpu2_monitor.liveness_count = 0U;

    // Command/fault status starts clean (MSGRAM is NOINIT).
    g_cpu2_monitor.cmd_ack_seq = 0U;
    g_cpu2_monitor.fault_code = CPU2_FAULT_NONE;
    g_cpu2_monitor.applied_enable = 0U;
    g_cpu2_monitor.applied_mode = 0U;
    g_cpu2_monitor.applied_setpoint = 0U;
}

void intercore_cpu2_service(void)
{
    if (g_cpu2_local_handshake_status != CPU2_LOCAL_HANDSHAKE_STATUS_ACK_SENT)
    {
        if (g_cpu2_command.mbox == CPU1_TO_CPU2_BOOT_READY_TOKEN)
        {
            g_cpu2_monitor.mbox = CPU2_TO_CPU1_ACK_READY_TOKEN;
            g_cpu2_local_handshake_status =
                CPU2_LOCAL_HANDSHAKE_STATUS_ACK_SENT;
        }
    }
    else
    {
        //
        // Handshake 완료 후에는 CPU2->CPU1 mailbox를 단조 증가 heartbeat로
        // 재사용한다. CPU1 supervisor는 이 값의 변화로 CPU2 생존을 판단한다.
        //
        // 이 갱신은 ACK를 쓴 그 다음 service 호출(루프 주기 >=100ms 후)부터
        // 일어나므로, CPU1 handshake polling이 ACK token을 latch하기 전에
        // 덮어쓰는 race가 없다.
        //
        if (g_cpu2_liveness_count < CPU2_LIVENESS_HEARTBEAT_START)
        {
            g_cpu2_liveness_count = CPU2_LIVENESS_HEARTBEAT_START;
        }
        else
        {
            ++g_cpu2_liveness_count;
        }

        g_cpu2_monitor.mbox = g_cpu2_liveness_count;
    }

    ++g_cpu2_tick_count;

    // Refresh the easyDSP monitor mirror (read by CPU1's single SCI-A module).
    g_cpu2_monitor.main_entered = g_cpu2_main_entered;
    g_cpu2_monitor.tick_count = g_cpu2_tick_count;
    g_cpu2_monitor.local_handshake_status = g_cpu2_local_handshake_status;
    g_cpu2_monitor.liveness_count = g_cpu2_liveness_count;

    // Apply CPU1 -> CPU2 commands once the handshake is up (Stage 1: latch,
    // acknowledge, and raise a fault on e-stop). Wire enable/mode/setpoint to the
    // control loop when it exists.
    if (g_cpu2_local_handshake_status == CPU2_LOCAL_HANDSHAKE_STATUS_ACK_SENT)
    {
        g_cpu2_monitor.applied_enable = g_cpu2_command.enable;
        g_cpu2_monitor.applied_mode = g_cpu2_command.mode;
        g_cpu2_monitor.applied_setpoint = g_cpu2_command.setpoint;
        g_cpu2_monitor.fault_code =
            (g_cpu2_command.estop != 0U) ? CPU2_FAULT_ESTOP : CPU2_FAULT_NONE;
        g_cpu2_monitor.cmd_ack_seq = g_cpu2_command.seq;
    }
}
