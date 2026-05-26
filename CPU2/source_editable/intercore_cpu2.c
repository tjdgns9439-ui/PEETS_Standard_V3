#include "../CPU2/include_editable/initial_header.h"
#include "../CPU2/include_editable/intercore_cpu2.h"

#define CPU1_TO_CPU2_BOOT_READY_TOKEN 0xC002CAFEUL
#define CPU2_TO_CPU1_ACK_READY_TOKEN 0xACCE2C11UL
#define CPU1_CPU2_IPC_DRIVERLIB_BUFFER_WORDS 68U

typedef struct
{
    uint32_t ipc_driverlib_buffer_pad[CPU1_CPU2_IPC_DRIVERLIB_BUFFER_WORDS];
    uint32_t mailbox;
} IntercoreCpu2MsgramMailbox;

volatile uint32_t g_cpu2_main_entered = 0U;
volatile uint32_t g_cpu2_tick_count = 0U;
volatile uint32_t g_cpu2_handshake_status = 0U;

#pragma DATA_SECTION(g_cpu1_to_cpu2_msgram, "MSGRAM_CPU1_TO_CPU2")
volatile IntercoreCpu2MsgramMailbox g_cpu1_to_cpu2_msgram;

#pragma DATA_SECTION(g_cpu2_to_cpu1_msgram, "MSGRAM_CPU2_TO_CPU1")
volatile IntercoreCpu2MsgramMailbox g_cpu2_to_cpu1_msgram;

void intercore_cpu2_init(void)
{
    g_cpu2_main_entered = 1U;
    g_cpu2_handshake_status = CPU2_LOCAL_HANDSHAKE_STATUS_WAITING_TOKEN;
}

void intercore_cpu2_service(void)
{
    if (g_cpu1_to_cpu2_msgram.mailbox == CPU1_TO_CPU2_BOOT_READY_TOKEN)
    {
        g_cpu2_to_cpu1_msgram.mailbox = CPU2_TO_CPU1_ACK_READY_TOKEN;
        g_cpu2_handshake_status = CPU2_LOCAL_HANDSHAKE_STATUS_ACK_SENT;
    }

    ++g_cpu2_tick_count;
}
