#include "../CPU1/include_editable/initial_header.h"
#include "../CPU1/include_editable/pwm.h"

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

extern "C" uint16_t pwm_test_run(uint16_t seed)
{
    return static_cast<uint16_t>(seed + 1U);
}
