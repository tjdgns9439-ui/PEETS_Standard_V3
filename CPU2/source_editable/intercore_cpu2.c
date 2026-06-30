#include "../CPU2/include_editable/initial_header.h"
#include "../CPU2/include_editable/intercore_cpu2.h"

#define CPU1_TO_CPU2_BOOT_READY_TOKEN 0xC002CAFEUL
#define CPU2_TO_CPU1_ACK_READY_TOKEN 0xACCE2C11UL
#define CPU2_LIVENESS_HEARTBEAT_START 1U

volatile uint32_t g_cpu2_main_entered = 0U;
volatile uint32_t g_cpu2_tick_count = 0U;
volatile uint32_t g_cpu2_local_handshake_status = 0U;
volatile uint32_t g_cpu2_liveness_count = 0U;

// MSGRAM mailbox는 CPU1 쪽(intercore_cpu1.c)과 같은 주소(section offset 0)를 보도록
// plain word로 둔다. raw mailbox polling만 쓰므로 driverlib buffer pad는 두지 않는다.
#pragma DATA_SECTION(g_cpu1_to_cpu2_mailbox, "MSGRAM_CPU1_TO_CPU2")
volatile uint32_t g_cpu1_to_cpu2_mailbox;

#pragma DATA_SECTION(g_cpu2_to_cpu1_mailbox, "MSGRAM_CPU2_TO_CPU1")
volatile uint32_t g_cpu2_to_cpu1_mailbox;

void intercore_cpu2_init(void)
{
    g_cpu2_main_entered = 1U;
    g_cpu2_local_handshake_status = CPU2_LOCAL_HANDSHAKE_STATUS_WAITING_TOKEN;
}

void intercore_cpu2_service(void)
{
    if (g_cpu2_local_handshake_status != CPU2_LOCAL_HANDSHAKE_STATUS_ACK_SENT)
    {
        if (g_cpu1_to_cpu2_mailbox == CPU1_TO_CPU2_BOOT_READY_TOKEN)
        {
            g_cpu2_to_cpu1_mailbox = CPU2_TO_CPU1_ACK_READY_TOKEN;
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

        g_cpu2_to_cpu1_mailbox = g_cpu2_liveness_count;
    }

    ++g_cpu2_tick_count;
}
