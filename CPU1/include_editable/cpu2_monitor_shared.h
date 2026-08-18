#ifndef CPU1_INCLUDE_EDITABLE_CPU2_MONITOR_SHARED_H_
#define CPU1_INCLUDE_EDITABLE_CPU2_MONITOR_SHARED_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CPU1 <-> CPU2 shared IPC blocks (polling / message-RAM based).
 *
 * Both structs live in the app MSGRAM sections placed at the HIGH end of their
 * message-RAM region (see the linker cmd files and intercore_*.c). HIGH + equal
 * region/struct size => the CPU1 and CPU2 builds resolve them to the SAME
 * physical address, so both cores agree on every field.
 *
 * Direction / ownership (message RAM is hardware R/W one way):
 *   g_cpu2_command  CPU1 -> CPU2   CPU1 writes, CPU2 reads   (commands)
 *   g_cpu2_monitor  CPU2 -> CPU1   CPU2 writes, CPU1 reads   (status/telemetry)
 *
 * easyDSP monitors through CPU1's single SCI-A: it READS g_cpu2_monitor.* and
 * (because CPU1 owns the write side) can WRITE g_cpu2_command.* to command CPU2.
 *
 * Field [0] (mbox) of each struct is the original boot-handshake mailbox word
 * and MUST stay first so the handshake address is unchanged.
 */

/*
 * CPU1 -> CPU2 command block. easyDSP (or CPU1 logic) writes these; CPU2 reads
 * and applies them. Bump `seq` after changing fields to get an explicit ACK
 * (CPU2 echoes seq into g_cpu2_monitor.cmd_ack_seq once it has applied them).
 */
typedef struct
{
    volatile uint32_t mbox;      /* [0]  CPU1->CPU2 boot handshake token         */
    volatile uint32_t seq;       /* [1]  command sequence; CPU2 echoes to ack    */
    volatile uint32_t enable;    /* [2]  output/control enable request (0/1)     */
    volatile uint32_t mode;      /* [3]  control mode                            */
    volatile uint32_t setpoint;  /* [4]  control setpoint (raw)                  */
    volatile uint32_t estop;     /* [5]  1 = emergency stop request              */
    volatile uint32_t param[10]; /* [6..15] spare command parameters             */
} cpu2_command_t;

/*
 * CPU2 -> CPU1 status/telemetry block. CPU2 writes; CPU1 / easyDSP read.
 */
typedef struct
{
    volatile uint32_t mbox;                   /* [0]  handshake ACK token, then liveness heartbeat */
    volatile uint32_t main_entered;           /* [1]  mirror of g_cpu2_main_entered */
    volatile uint32_t tick_count;             /* [2]  mirror of g_cpu2_tick_count */
    volatile uint32_t local_handshake_status; /* [3]  mirror of g_cpu2_local_handshake_status */
    volatile uint32_t liveness_count;         /* [4]  mirror of g_cpu2_liveness_count */
    volatile uint32_t cmd_ack_seq;            /* [5]  echoes g_cpu2_command.seq once applied (ACK) */
    volatile uint32_t fault_code;             /* [6]  0 = ok, nonzero = CPU2 fault */
    volatile uint32_t applied_enable;         /* [7]  command CPU2 is currently applying */
    volatile uint32_t applied_mode;           /* [8] */
    volatile uint32_t applied_setpoint;       /* [9] */
    volatile uint32_t spare[6];               /* [10..15] free slots */
} cpu2_monitor_t;

/* CPU2 fault codes reported in g_cpu2_monitor.fault_code. */
#define CPU2_FAULT_NONE  0U
#define CPU2_FAULT_ESTOP 1U

extern volatile cpu2_command_t g_cpu2_command; /* CPU1 -> CPU2 */
extern volatile cpu2_monitor_t g_cpu2_monitor; /* CPU2 -> CPU1 */

#ifdef __cplusplus
}
#endif

#endif /* CPU1_INCLUDE_EDITABLE_CPU2_MONITOR_SHARED_H_ */
