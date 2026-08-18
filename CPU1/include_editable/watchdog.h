#ifndef CPU1_INCLUDE_EDITABLE_WATCHDOG_H_
#define CPU1_INCLUDE_EDITABLE_WATCHDOG_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Compile-time watchdog master switch (set before building):
 *   1 = watchdog ON  : hardware WD armed + serviced (a code hang resets CPU1 in
 *                      ~0.84 s), and the system-fault escalation can reset the
 *                      device after its timeout.
 *   0 = watchdog OFF : WD never armed, never serviced, and watchdog_force_reset()
 *                      does nothing -> NO automatic CPU resets (handy for JTAG
 *                      debugging, where halting at a breakpoint would otherwise
 *                      trip the WD). Safe-off / fault safe-state still work.
 */
#define CPU1_WATCHDOG_ENABLE 0U

/*
 * CPU1 watchdog + reset-cause reporting.
 *
 * The watchdog resets CPU1 if watchdog_service() is not called within the
 * timeout (~0.84 s, clocked from INTOSC1, independent of SYSCLK). Call
 * watchdog_service() often enough everywhere the CPU can block for a while
 * (the main-loop delay and the intercore handshake poll loops already do).
 *
 * On boot, watchdog_init() latches WHY CPU1 last reset into g_cpu1_reset_cause
 * (raw SysCtl_getResetCause() mask) and sets g_cpu1_reset_was_watchdog=1 if it
 * was a watchdog reset, then re-arms the watchdog. Watch these in easyDSP to
 * see whether the board has been resetting and why.
 *
 * Raw cause bits (SYSCTL_CAUSE_*): POR=power-on, XRS=external reset pin,
 * WDRS=watchdog, NMIWDRS=NMI watchdog, SCCRESET.
 */
extern volatile uint32_t g_cpu1_reset_cause;         /* raw reset-cause mask   */
extern volatile uint32_t g_cpu1_reset_was_watchdog;  /* 1 if last reset = WD   */

void watchdog_init(void);     /* capture reset cause, configure + enable WD */
void watchdog_service(void);  /* "pet" the watchdog (safe if not yet enabled) */

/*
 * Force a full device restart NOW. Puts CPU2/CM into reset and stops petting the
 * (already-armed) watchdog, so CPU1's watchdog resets the device within ~0.84 s;
 * CPU1's reboot then re-boots CPU2/CM. Does NOT return.
 */
void watchdog_force_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* CPU1_INCLUDE_EDITABLE_WATCHDOG_H_ */
