#ifndef CPU1_INCLUDE_EDITABLE_INTERCORE_CPU1_H_
#define CPU1_INCLUDE_EDITABLE_INTERCORE_CPU1_H_

#include <stdint.h>

#include "../CPU1/include_editable/cpu2_monitor_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

enum
{
    CM_HANDSHAKE_STATUS_IDLE = 0U,
    CM_HANDSHAKE_STATUS_BOOTED = 1U,
    CM_HANDSHAKE_STATUS_WAITING_ACK = 2U,
    CM_HANDSHAKE_STATUS_OK = 3U,
    CM_HANDSHAKE_STATUS_TIMEOUT = 4U
};

enum
{
    CPU2_HANDSHAKE_STATUS_IDLE = 0U,
    CPU2_HANDSHAKE_STATUS_BOOTED = 1U,
    CPU2_HANDSHAKE_STATUS_WAITING_ACK = 2U,
    CPU2_HANDSHAKE_STATUS_OK = 3U,
    CPU2_HANDSHAKE_STATUS_TIMEOUT = 4U
};

extern volatile uint32_t g_cm_handshake_status;
extern volatile uint32_t g_cm_handshake_poll_count;
extern volatile uint32_t g_cpu2_handshake_status;
extern volatile uint32_t g_cpu2_handshake_poll_count;

extern volatile uint32_t g_cpu1_to_cm_mailbox;
extern volatile uint32_t g_cm_to_cpu1_mailbox;
/* CPU1<->CPU2 mailboxes are now g_cpu2_command.mbox / g_cpu2_monitor.mbox
 * (see cpu2_monitor_shared.h). */

void intercore_cpu1_run_cm_handshake(void);
void intercore_cpu1_run_cpu2_handshake(void);

#ifdef __cplusplus
}
#endif

#endif
