#ifndef CPU2_INCLUDE_EDITABLE_INTERCORE_CPU2_H_
#define CPU2_INCLUDE_EDITABLE_INTERCORE_CPU2_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum
{
    CPU2_LOCAL_HANDSHAKE_STATUS_WAITING_TOKEN = 1U,
    CPU2_LOCAL_HANDSHAKE_STATUS_ACK_SENT = 2U
};

extern volatile uint32_t g_cpu2_main_entered;
extern volatile uint32_t g_cpu2_tick_count;
extern volatile uint32_t g_cpu2_handshake_status;

void intercore_cpu2_init(void);
void intercore_cpu2_service(void);

#ifdef __cplusplus
}
#endif

#endif
