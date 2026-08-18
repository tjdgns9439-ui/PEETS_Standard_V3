#ifndef CPU1_INCLUDE_EDITABLE_LOOP_TIMING_H_
#define CPU1_INCLUDE_EDITABLE_LOOP_TIMING_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Reusable loop timing / jitter monitor.
 *
 * Uses CPU Timer 0 as a free-running 32-bit down-counter at SYSCLK, so it
 * measures wall-clock time between calls with cycle resolution. All results are
 * in microseconds and exposed via g_cpu1_loop_timing for easyDSP.
 *
 * Usage (drop into any periodic loop or control ISR):
 *     loop_timing_init();                 // once, before the loop
 *     for (;;) {
 *         loop_timing_loop_begin();       // top of loop  -> period + jitter
 *         ... do the work ...
 *         loop_timing_work_end();         // after work   -> exec time
 *         DEVICE_DELAY_US(...);           // (or wait-for-trigger)
 *     }
 *
 *  - period = time between successive loop_begin calls (full loop rate)
 *  - exec   = loop_begin -> work_end (work only, excludes the wait/delay)
 *  - jitter = max_period - min_period
 */
typedef struct
{
    uint32_t loop_count;      /* number of completed iterations                */
    uint32_t last_period_us;  /* most recent loop period                       */
    uint32_t min_period_us;   /* smallest period seen                          */
    uint32_t max_period_us;   /* largest period seen                           */
    uint32_t jitter_us;       /* max_period_us - min_period_us                 */
    uint32_t last_exec_us;    /* most recent work time (begin -> work_end)     */
    uint32_t max_exec_us;     /* largest work time seen                        */
} loop_timing_t;

extern volatile loop_timing_t g_cpu1_loop_timing;

void loop_timing_init(void);
void loop_timing_loop_begin(void);
void loop_timing_work_end(void);

#ifdef __cplusplus
}
#endif

#endif /* CPU1_INCLUDE_EDITABLE_LOOP_TIMING_H_ */
