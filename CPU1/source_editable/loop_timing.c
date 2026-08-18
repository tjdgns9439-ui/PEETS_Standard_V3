#include "../CPU1/include_editable/initial_header.h"
#include "../CPU1/include_editable/loop_timing.h"

// SYSCLK cycles per microsecond (200 at 200 MHz). CPU Timer 0 clocks at SYSCLK.
#define LOOP_TIMING_CYCLES_PER_US (DEVICE_SYSCLK_FREQ / 1000000UL)

volatile loop_timing_t g_cpu1_loop_timing;

static uint32_t s_prev_begin_count; // timer count at the previous loop_begin
static uint32_t s_work_start_count; // timer count at this loop_begin (for exec)
static uint16_t s_have_prev;        // 0 until the first loop_begin has run

static uint32_t loop_timing_elapsed_us(uint32_t start, uint32_t end)
{
    // CPU Timer 0 is a DOWN counter, so elapsed cycles = start - end (mod 2^32),
    // which stays correct across a counter reload.
    uint32_t cycles = (uint32_t)(start - end);
    return cycles / LOOP_TIMING_CYCLES_PER_US;
}

void loop_timing_init(void)
{
    g_cpu1_loop_timing.loop_count = 0U;
    g_cpu1_loop_timing.last_period_us = 0U;
    g_cpu1_loop_timing.min_period_us = 0xFFFFFFFFU;
    g_cpu1_loop_timing.max_period_us = 0U;
    g_cpu1_loop_timing.jitter_us = 0U;
    g_cpu1_loop_timing.last_exec_us = 0U;
    g_cpu1_loop_timing.max_exec_us = 0U;

    s_have_prev = 0U;

    // CPU Timer 0: free-running 32-bit down-counter at SYSCLK (prescaler /1).
    CPUTimer_stopTimer(CPUTIMER0_BASE);
    CPUTimer_setPeriod(CPUTIMER0_BASE, 0xFFFFFFFFU);
    CPUTimer_setPreScaler(CPUTIMER0_BASE, 0U);
    CPUTimer_reloadTimerCounter(CPUTIMER0_BASE);
    CPUTimer_setEmulationMode(CPUTIMER0_BASE, CPUTIMER_EMULATIONMODE_RUNFREE);
    CPUTimer_disableInterrupt(CPUTIMER0_BASE);
    CPUTimer_startTimer(CPUTIMER0_BASE);
}

void loop_timing_loop_begin(void)
{
    uint32_t now = CPUTimer_getTimerCount(CPUTIMER0_BASE);

    if (s_have_prev != 0U)
    {
        uint32_t period_us = loop_timing_elapsed_us(s_prev_begin_count, now);

        g_cpu1_loop_timing.last_period_us = period_us;
        if (period_us < g_cpu1_loop_timing.min_period_us)
        {
            g_cpu1_loop_timing.min_period_us = period_us;
        }
        if (period_us > g_cpu1_loop_timing.max_period_us)
        {
            g_cpu1_loop_timing.max_period_us = period_us;
        }
        g_cpu1_loop_timing.jitter_us =
            g_cpu1_loop_timing.max_period_us - g_cpu1_loop_timing.min_period_us;
        ++g_cpu1_loop_timing.loop_count;
    }
    else
    {
        s_have_prev = 1U;
    }

    s_prev_begin_count = now;
    s_work_start_count = now;
}

void loop_timing_work_end(void)
{
    uint32_t now = CPUTimer_getTimerCount(CPUTIMER0_BASE);
    uint32_t exec_us = loop_timing_elapsed_us(s_work_start_count, now);

    g_cpu1_loop_timing.last_exec_us = exec_us;
    if (exec_us > g_cpu1_loop_timing.max_exec_us)
    {
        g_cpu1_loop_timing.max_exec_us = exec_us;
    }
}
