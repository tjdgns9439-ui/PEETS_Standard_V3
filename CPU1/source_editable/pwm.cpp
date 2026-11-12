#include "../CPU1/include_editable/initial_header.h"
#include "../CPU1/include_editable/pwm.h"

// Live PWM config (write from easyDSP). C linkage so easyDSP resolves the symbol.
// Boot default is OFF: set g_pwm_config.enable = 1 (and the board buffers via
// g_board_io_enable_request = 1) from easyDSP to start switching.
extern "C" volatile pwm_config_t g_pwm_config = {
    0U,                        // enable          : off at boot (safe)
    360000U,                   // freq_hz         : 360 kHz
    100U,                      // deadtime_on_ns  : 100 ns (RED)
    100U,                      // deadtime_off_ns : 100 ns (FED)
    PWM_BRIDGE1 | PWM_BRIDGE2, // channel_sel     : both full bridges
    {0.5f, 0.5f, 0.5f, 0.5f},  // duty[]          : 50 % on every leg
    // phase_deg[]: each bridge outputs a full square wave (legs 180 deg apart),
    // bridge 2 in phase with bridge 1.
    {0.0f, 180.0f, 0.0f, 180.0f}
};

// Live PWM status (read from easyDSP): running or not, and why not.
extern "C" volatile pwm_status_t g_pwm_status = {
    {0U, 0U, 0U, 0U}, // leg_switching
    0U,               // output_at_board
    PWM_OFF_DISABLED | PWM_OFF_BUFFER, // off_reason (boot state)
    0U                // fault_latched
};

#define PWM_SMOKE_PERIOD_TICKS 50000U
#define PWM_SMOKE_CMPA_TICKS (PWM_SMOKE_PERIOD_TICKS / 4U)
#define PWM_SMOKE_CMPB_TICKS (PWM_SMOKE_PERIOD_TICKS / 2U)

static const uint32_t kEpwmBases[] = {
    EPWM1_BASE, EPWM2_BASE, EPWM3_BASE, EPWM4_BASE,
    EPWM5_BASE, EPWM6_BASE, EPWM7_BASE, EPWM8_BASE,
    EPWM9_BASE, EPWM10_BASE, EPWM11_BASE, EPWM12_BASE
};

static void pwm_smoke_configure_one(uint32_t base)
{
    EPWM_setEmulationMode(base, EPWM_EMULATION_FREE_RUN);
    EPWM_setClockPrescaler(base, EPWM_CLOCK_DIVIDER_1,
                           EPWM_HSCLOCK_DIVIDER_1);
    EPWM_setTimeBasePeriod(base, PWM_SMOKE_PERIOD_TICKS);
    EPWM_setTimeBaseCounter(base, 0U);
    EPWM_setTimeBaseCounterMode(base, EPWM_COUNTER_MODE_UP);
    EPWM_setCounterCompareValue(base, EPWM_COUNTER_COMPARE_A,
                                PWM_SMOKE_CMPA_TICKS);
    EPWM_setCounterCompareValue(base, EPWM_COUNTER_COMPARE_B,
                                PWM_SMOKE_CMPB_TICKS);
    EPWM_setCounterCompareShadowLoadMode(base, EPWM_COUNTER_COMPARE_A,
                                         EPWM_COMP_LOAD_ON_CNTR_ZERO);
    EPWM_setCounterCompareShadowLoadMode(base, EPWM_COUNTER_COMPARE_B,
                                         EPWM_COMP_LOAD_ON_CNTR_ZERO);

    EPWM_setActionQualifierAction(base, EPWM_AQ_OUTPUT_A,
                                  EPWM_AQ_OUTPUT_LOW,
                                  EPWM_AQ_OUTPUT_ON_TIMEBASE_PERIOD);
    EPWM_setActionQualifierAction(base, EPWM_AQ_OUTPUT_A,
                                  EPWM_AQ_OUTPUT_HIGH,
                                  EPWM_AQ_OUTPUT_ON_TIMEBASE_ZERO);
    EPWM_setActionQualifierAction(base, EPWM_AQ_OUTPUT_A,
                                  EPWM_AQ_OUTPUT_LOW,
                                  EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);

    EPWM_setActionQualifierAction(base, EPWM_AQ_OUTPUT_B,
                                  EPWM_AQ_OUTPUT_LOW,
                                  EPWM_AQ_OUTPUT_ON_TIMEBASE_PERIOD);
    EPWM_setActionQualifierAction(base, EPWM_AQ_OUTPUT_B,
                                  EPWM_AQ_OUTPUT_HIGH,
                                  EPWM_AQ_OUTPUT_ON_TIMEBASE_ZERO);
    EPWM_setActionQualifierAction(base, EPWM_AQ_OUTPUT_B,
                                  EPWM_AQ_OUTPUT_LOW,
                                  EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPB);

    EPWM_setRisingEdgeDelayCount(base, 0U);
    EPWM_setFallingEdgeDelayCount(base, 0U);
    EPWM_setDeadBandDelayMode(base, EPWM_DB_RED, false);
    EPWM_setDeadBandDelayMode(base, EPWM_DB_FED, false);
    EPWM_disablePhaseShiftLoad(base);

    EPWM_setTripZoneAction(base, EPWM_TZ_ACTION_EVENT_TZA,
                           EPWM_TZ_ACTION_LOW);
    EPWM_setTripZoneAction(base, EPWM_TZ_ACTION_EVENT_TZB,
                           EPWM_TZ_ACTION_LOW);
    EPWM_forceTripZoneEvent(base, EPWM_TZ_FORCE_EVENT_OST);
}

extern "C" void pwm_smoke_init(void)
{
    uint16_t i;

    SysCtl_disablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);

    for (i = 0U; i < (sizeof(kEpwmBases) / sizeof(kEpwmBases[0])); ++i)
    {
        pwm_smoke_configure_one(kEpwmBases[i]);
    }

    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);
}

extern "C" void pwm_smoke_force_safe_off(void)
{
    uint16_t i;

    for (i = 0U; i < (sizeof(kEpwmBases) / sizeof(kEpwmBases[0])); ++i)
    {
        EPWM_forceTripZoneEvent(kEpwmBases[i], EPWM_TZ_FORCE_EVENT_OST);
    }
}

extern "C" void pwm_smoke_release_outputs(void)
{
    uint16_t i;

    for (i = 0U; i < (sizeof(kEpwmBases) / sizeof(kEpwmBases[0])); ++i)
    {
        EPWM_clearTripZoneFlag(kEpwmBases[i],
                               EPWM_TZ_INTERRUPT | EPWM_TZ_FLAG_OST);
    }
}

// ---------------------------------------------------------------------------
// Two full bridges on EPWM1..4, driven by g_pwm_config.
//   Bridge 1 = EPWM1 + EPWM2, bridge 2 = EPWM3 + EPWM4 (one EPWM per leg).
//   EPWMxA = HIGH side, EPWMxB = LOW side. Center-aligned (up-down) with
//   complementary dead-band (A/B never on together -> no shoot-through).
//   EPWMxA is HIGH centered on CTR = 0, for duty * period.
//
//   EPWM1 is the time-base master (sync-out at CTR = 0). EPWM2..4 take that
//   sync pulse and reload their counter from TBPHS, which sets the per-leg
//   phase (phase_deg[]). A phase change takes effect at the next master zero.
//
//   FREQUENCY is applied ONLY in pwm_config_init() (changing it while the stage
//   runs is unsafe). Duty / phase / dead-times / enable / channel_sel apply live
//   in pwm_config_service().
//
//   EPWMCLK = SYSCLK/2 = 100 MHz; TBCLK = EPWMCLK. Up-down: freq = TBCLK/(2*TBPRD).
//   Dead-band count is TBCLK cycles (100 MHz -> 10 ns/count). If measured freq is
//   2x off, EPWMCLK is 200 MHz -> update PWM_EPWMCLK_HZ.
//   Resolution is one TBCLK: at 360 kHz TBPRD = 139, so duty steps are ~0.7 % and
//   phase steps ~1.3 deg (HRPWM is not used yet).
// ---------------------------------------------------------------------------
#define PWM_EPWMCLK_HZ 100000000UL
// Sync pulse master -> slave latency when TBCLK = EPWMCLK (TBCLK cycles).
#define PWM_SYNC_DELAY_TICKS 2

static const uint32_t kPwmBases[PWM_LEG_COUNT] = {EPWM1_BASE, EPWM2_BASE,
                                                  EPWM3_BASE, EPWM4_BASE};
static const uint32_t kPwmChBit[PWM_LEG_COUNT] = {PWM_CH_EPWM1, PWM_CH_EPWM2,
                                                  PWM_CH_EPWM3, PWM_CH_EPWM4};
static uint16_t s_pwm_tbprd = 1U; // frequency (set once in pwm_config_init)

static uint16_t pwm_ns_to_count(uint32_t ns)
{
    // TBCLK cycles for a nanosecond delay (100 MHz -> 10 ns per count).
    return static_cast<uint16_t>((ns * (PWM_EPWMCLK_HZ / 1000000U)) / 1000U);
}

static uint16_t pwm_duty_to_cmpa(float duty)
{
    if (!(duty > 0.0f)) // also catches NaN
    {
        return 0U;
    }
    if (duty >= 1.0f)
    {
        return s_pwm_tbprd;
    }
    return static_cast<uint16_t>((duty * static_cast<float>(s_pwm_tbprd)) + 0.5f);
}

// Program the counter value / direction a slave leg loads at the master's
// CTR = 0 so that it lags the master by phase_deg (negative = leads).
static void pwm_apply_phase(uint32_t base, float phase_deg)
{
    const int32_t prd = static_cast<int32_t>(s_pwm_tbprd);
    const int32_t cycle = 2 * prd; // TBCLK ticks per PWM period
    int32_t lag;
    int32_t t;

    if (!(phase_deg > -180.0f)) // also catches NaN
    {
        phase_deg = -180.0f;
    }
    if (phase_deg > 180.0f)
    {
        phase_deg = 180.0f;
    }
    lag = static_cast<int32_t>((phase_deg * static_cast<float>(prd)) / 180.0f +
                               (phase_deg >= 0.0f ? 0.5f : -0.5f));

    // Slave position (ticks since its own CTR = 0) at the moment it reloads.
    t = (PWM_SYNC_DELAY_TICKS - lag) % cycle;
    if (t < 0)
    {
        t += cycle;
    }

    if (t <= prd)
    {
        EPWM_setCountModeAfterSync(base, EPWM_COUNT_MODE_UP_AFTER_SYNC);
        EPWM_setPhaseShift(base, static_cast<uint16_t>(t));
    }
    else
    {
        EPWM_setCountModeAfterSync(base, EPWM_COUNT_MODE_DOWN_AFTER_SYNC);
        EPWM_setPhaseShift(base, static_cast<uint16_t>(cycle - t));
    }
}

// Duty, phase and dead-times (everything live except frequency and on/off).
static void pwm_apply_live(void)
{
    uint16_t red = pwm_ns_to_count(g_pwm_config.deadtime_on_ns);
    uint16_t fed = pwm_ns_to_count(g_pwm_config.deadtime_off_ns);
    uint16_t i;

    for (i = 0U; i < PWM_LEG_COUNT; ++i)
    {
        uint32_t base = kPwmBases[i];

        EPWM_setCounterCompareValue(base, EPWM_COUNTER_COMPARE_A,
                                    pwm_duty_to_cmpa(g_pwm_config.duty[i]));
        EPWM_setRisingEdgeDelayCount(base, red);
        EPWM_setFallingEdgeDelayCount(base, fed);
        if (i != 0U) // EPWM1 is the phase reference
        {
            pwm_apply_phase(base, g_pwm_config.phase_deg[i]);
        }
    }
}

static void pwm_config_module_static(uint32_t base, uint16_t tbprd,
                                     bool is_master)
{
    EPWM_setEmulationMode(base, EPWM_EMULATION_FREE_RUN);
    EPWM_setClockPrescaler(base, EPWM_CLOCK_DIVIDER_1, EPWM_HSCLOCK_DIVIDER_1);
    EPWM_setTimeBasePeriod(base, tbprd); // frequency (init-only)
    EPWM_setTimeBaseCounter(base, 0U);
    EPWM_setTimeBaseCounterMode(base, EPWM_COUNTER_MODE_UP_DOWN); // symmetric
    EPWM_setCounterCompareShadowLoadMode(base, EPWM_COUNTER_COMPARE_A,
                                         EPWM_COMP_LOAD_ON_CNTR_ZERO);
    // EPWMA (center-aligned): HIGH on down-count CMPA, LOW on up-count CMPA.
    // Clear the ZERO / PERIOD actions left over from the smoke profile.
    EPWM_setActionQualifierAction(base, EPWM_AQ_OUTPUT_A,
                                  EPWM_AQ_OUTPUT_NO_CHANGE,
                                  EPWM_AQ_OUTPUT_ON_TIMEBASE_ZERO);
    EPWM_setActionQualifierAction(base, EPWM_AQ_OUTPUT_A,
                                  EPWM_AQ_OUTPUT_NO_CHANGE,
                                  EPWM_AQ_OUTPUT_ON_TIMEBASE_PERIOD);
    EPWM_setActionQualifierAction(base, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_HIGH,
                                  EPWM_AQ_OUTPUT_ON_TIMEBASE_DOWN_CMPA);
    EPWM_setActionQualifierAction(base, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_LOW,
                                  EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);
    // Active-high complementary: A = high side, B = inverted A (low side).
    // Both dead-band edges are derived from EPWMxA.
    EPWM_setRisingEdgeDeadBandDelayInput(base, EPWM_DB_INPUT_EPWMA);
    EPWM_setFallingEdgeDeadBandDelayInput(base, EPWM_DB_INPUT_EPWMA);
    EPWM_setDeadBandDelayMode(base, EPWM_DB_RED, true);
    EPWM_setDeadBandDelayMode(base, EPWM_DB_FED, true);
    EPWM_setDeadBandDelayPolarity(base, EPWM_DB_RED,
                                  EPWM_DB_POLARITY_ACTIVE_HIGH);
    EPWM_setDeadBandDelayPolarity(base, EPWM_DB_FED,
                                  EPWM_DB_POLARITY_ACTIVE_LOW);
    // Time-base synchronization: EPWM1 = master, the others reload on its sync.
    if (is_master)
    {
        EPWM_disablePhaseShiftLoad(base);
        EPWM_setPhaseShift(base, 0U);
        EPWM_enableSyncOutPulseSource(base, EPWM_SYNC_OUT_PULSE_ON_CNTR_ZERO);
    }
    else
    {
        EPWM_setSyncInPulseSource(base, EPWM_SYNC_IN_PULSE_SRC_SYNCOUT_EPWM1);
        EPWM_enablePhaseShiftLoad(base);
    }
    // A trip forces both outputs LOW (safe off).
    EPWM_setTripZoneAction(base, EPWM_TZ_ACTION_EVENT_TZA, EPWM_TZ_ACTION_LOW);
    EPWM_setTripZoneAction(base, EPWM_TZ_ACTION_EVENT_TZB, EPWM_TZ_ACTION_LOW);
    EPWM_forceTripZoneEvent(base, EPWM_TZ_FORCE_EVENT_OST); // start OFF
}

extern "C" void pwm_config_init(void)
{
    uint32_t freq = g_pwm_config.freq_hz;
    uint16_t i;

    if (freq < 1000U)
    {
        freq = 1000U; // clamp to avoid divide/overflow
    }
    s_pwm_tbprd =
        static_cast<uint16_t>((PWM_EPWMCLK_HZ + freq) / (2U * freq));

    SysCtl_disablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);
    for (i = 0U; i < PWM_LEG_COUNT; ++i)
    {
        pwm_config_module_static(kPwmBases[i], s_pwm_tbprd, i == 0U);
    }
    pwm_apply_live(); // duty / phase / dead-time in place before the clock runs
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);

    // Let the slaves take at least one sync from EPWM1 before any output can be
    // released, so the legs are already phase-aligned when they start.
    DEVICE_DELAY_US((3000000UL / freq) + 1UL);

    // Buffers are off at this point in boot; report it until main says otherwise.
    pwm_config_service(PWM_OFF_BUFFER); // apply enable / channel_sel now
}

extern "C" void pwm_config_service(uint16_t block_reasons)
{
    const uint16_t fault = block_reasons & PWM_OFF_FAULT_MASK;
    const uint32_t enable = g_pwm_config.enable;
    const uint32_t channel_sel = g_pwm_config.channel_sel;
    uint16_t reason = block_reasons;
    uint16_t any_switching = 0U;
    uint16_t i;

    pwm_apply_live();

    for (i = 0U; i < PWM_LEG_COUNT; ++i)
    {
        uint32_t base = kPwmBases[i];
        uint16_t switching;

        // Output on/off: user enable + channel select, gated by faults.
        if ((fault == 0U) && (enable != 0U) &&
            ((channel_sel & kPwmChBit[i]) != 0U))
        {
            EPWM_clearTripZoneFlag(base, EPWM_TZ_INTERRUPT | EPWM_TZ_FLAG_OST);
        }
        else
        {
            EPWM_forceTripZoneEvent(base, EPWM_TZ_FORCE_EVENT_OST);
        }

        // Report what the hardware is actually doing, not what was requested.
        switching =
            ((EPWM_getTripZoneFlagStatus(base) & EPWM_TZ_FLAG_OST) == 0U) ? 1U
                                                                         : 0U;
        g_pwm_status.leg_switching[i] = switching;
        any_switching |= switching;
    }

    if (enable == 0U)
    {
        reason |= PWM_OFF_DISABLED;
    }
    if ((channel_sel & (PWM_BRIDGE1 | PWM_BRIDGE2)) == 0U)
    {
        reason |= PWM_OFF_NO_CHANNEL;
    }

    g_pwm_status.off_reason = reason;
    g_pwm_status.fault_latched |= fault; // sticky until cleared from easyDSP
    g_pwm_status.output_at_board =
        ((any_switching != 0U) && ((block_reasons & PWM_OFF_BUFFER) == 0U))
            ? 1U
            : 0U;
}

extern "C" uint16_t pwm_get_period_ticks(void)
{
    return s_pwm_tbprd;
}

extern "C" uint16_t pwm_test_run(uint16_t seed)
{
    return static_cast<uint16_t>(seed + 1U);
}
