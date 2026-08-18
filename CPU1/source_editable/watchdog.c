#include "../CPU1/include_editable/initial_header.h"
#include "../CPU1/include_editable/watchdog.h"

/*
 * Watchdog timeout ~= 256 * PRESCALE * PREDIV / INTOSC1
 *                  =  256 *   64     *  512   / 10 MHz  ~= 0.84 s
 * The main loop and the handshake poll loops service it far more often than
 * this, so a real code hang trips it within ~0.84 s while normal operation
 * never false-trips.
 */
#define WATCHDOG_PREDIV   SYSCTL_WD_PREDIV_512
#define WATCHDOG_PRESCALE SYSCTL_WD_PRESCALE_64

volatile uint32_t g_cpu1_reset_cause;
volatile uint32_t g_cpu1_reset_was_watchdog;

void watchdog_init(void)
{
    //
    // Latch and report why CPU1 last reset, then clear the sticky cause bits so
    // the next boot reports a fresh reason.
    //
    uint32_t cause = SysCtl_getResetCause();

    g_cpu1_reset_cause = cause;
    g_cpu1_reset_was_watchdog = ((cause & SYSCTL_CAUSE_WDRS) != 0U) ? 1U : 0U;

    SysCtl_clearResetCause(SYSCTL_CAUSE_POR | SYSCTL_CAUSE_XRS |
                           SYSCTL_CAUSE_WDRS | SYSCTL_CAUSE_NMIWDRS |
                           SYSCTL_CAUSE_SCCRESET);

#if CPU1_WATCHDOG_ENABLE
    //
    // Configure and enable the watchdog in reset mode (~0.84 s timeout).
    // Device_init() disabled it earlier; re-arm it here.
    //
    SysCtl_setWatchdogPredivider(WATCHDOG_PREDIV);
    SysCtl_setWatchdogPrescaler(WATCHDOG_PRESCALE);
    SysCtl_setWatchdogMode(SYSCTL_WD_MODE_RESET);
    SysCtl_serviceWatchdog();
    SysCtl_enableWatchdog();
#else
    // Watchdog disabled at compile time -> keep it off (no auto-reset).
    SysCtl_disableWatchdog();
#endif
}

void watchdog_service(void)
{
#if CPU1_WATCHDOG_ENABLE
    SysCtl_serviceWatchdog();
#endif
}

void watchdog_force_reset(void)
{
#if CPU1_WATCHDOG_ENABLE
    DINT; // stop ISRs; the watchdog resets us regardless of interrupt state

    // Hold CPU2/CM in reset so CPU1's reboot brings them up clean (re-booted by
    // the intercore handshakes). Harmless if CPU1's WD reset already resets them.
    SysCtl_controlCPU2Reset(SYSCTL_CORE_ACTIVE);
    SysCtl_controlCMReset(SYSCTL_CORE_ACTIVE);

    // Stop servicing the armed watchdog -> device resets within ~0.84 s.
    for (;;)
    {
    }
#else
    // Watchdog disabled -> no automatic reset; caller stays in the safe state.
    return;
#endif
}
