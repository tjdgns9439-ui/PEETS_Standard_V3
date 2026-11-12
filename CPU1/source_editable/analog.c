#include "../CPU1/include_editable/initial_header.h"
#include "../CPU1/include_editable/analog.h"
#include "../CPU1/include_editable/pwm.h"

#define ANALOG_EPWMCLK_HZ 100000000UL /* must match PWM_EPWMCLK_HZ in pwm.cpp */
#define ANALOG_ADC_ACQPS 64U          /* sample window = 65 SYSCLK (325 ns)   */
#define ANALOG_ADC_MAXCODE 4095U      /* 12-bit full scale                    */
#define ANALOG_DAC_MAXCODE 4095U
#define ANALOG_TRIGGER_EPWM_BASE EPWM1_BASE

volatile analog_t g_analog = {
    {0U, 0U},     /* adc_raw        */
    {0.0f, 0.0f}, /* adc_volt       */
    3.3f,         /* vref_v: set to 3.0 if the board VREF switch selects REF5030 */
    0U,           /* sample_count   */
    0U,           /* overflow_count */
    0U,           /* sample_rate_hz */
    /* dac_mode: DAC1 mirrors Vout, DAC2 mirrors IL, DAC3 is manual */
    {ANALOG_DAC_MODE_ADC_VOUT, ANALOG_DAC_MODE_ADC_IL, ANALOG_DAC_MODE_MANUAL},
    {0U, 0U, 0U}  /* dac_code       */
};

static const uint32_t kDacBases[ANALOG_DAC_CH_COUNT] = {DACA_BASE, DACB_BASE,
                                                        DACC_BASE};

static void analog_init_adc_module(uint32_t base, ADC_Channel channel)
{
    ADC_setPrescaler(base, ADC_CLK_DIV_4_0); /* ADCCLK = 50 MHz */
    ADC_setMode(base, ADC_RESOLUTION_12BIT, ADC_MODE_SINGLE_ENDED);
    ADC_setInterruptPulseMode(base, ADC_PULSE_END_OF_CONV);
    ADC_enableConverter(base);
    ADC_setupSOC(base, ADC_SOC_NUMBER0, ADC_TRIGGER_EPWM1_SOCA, channel,
                 ANALOG_ADC_ACQPS);
}

static void analog_init_dac(void)
{
    uint16_t i;

    for (i = 0U; i < ANALOG_DAC_CH_COUNT; ++i)
    {
        DAC_setReferenceVoltage(kDacBases[i], DAC_REF_ADC_VREFHI);
        DAC_setLoadMode(kDacBases[i], DAC_LOAD_SYSCLK);
        DAC_setShadowValue(kDacBases[i], 0U);
        DAC_enableOutput(kDacBases[i]);
    }
    DEVICE_DELAY_US(10U); /* buffered DAC power-up */
}

static uint16_t analog_dac_value(uint16_t ch, uint16_t raw_vout,
                                 uint16_t raw_il)
{
    uint16_t code;

    switch (g_analog.dac_mode[ch])
    {
    case ANALOG_DAC_MODE_ADC_VOUT:
        code = raw_vout;
        break;
    case ANALOG_DAC_MODE_ADC_IL:
        code = raw_il;
        break;
    case ANALOG_DAC_MODE_MANUAL:
    default:
        code = g_analog.dac_code[ch];
        break;
    }

    return (code > ANALOG_DAC_MAXCODE) ? ANALOG_DAC_MAXCODE : code;
}

/*
 * Sample ISR: ADCA INT1 = end of the ADCINA2 conversion. ADCINB2 was started by
 * the same EPWM1 SOCA pulse with the same timing, so its result is ready too.
 * This is the place to add the control law later (keep it short and bounded).
 */
__interrupt void analog_adca1_isr(void)
{
    uint16_t raw_vout = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER0);
    uint16_t raw_il = ADC_readResult(ADCBRESULT_BASE, ADC_SOC_NUMBER0);
    /* Multiply by a constant reciprocal: no FP division inside the ISR. */
    float volt_per_code = g_analog.vref_v * (1.0f / (float)ANALOG_ADC_MAXCODE);
    uint16_t i;

    g_analog.adc_raw[ANALOG_ADC_CH_VOUT] = raw_vout;
    g_analog.adc_raw[ANALOG_ADC_CH_IL] = raw_il;
    g_analog.adc_volt[ANALOG_ADC_CH_VOUT] = (float)raw_vout * volt_per_code;
    g_analog.adc_volt[ANALOG_ADC_CH_IL] = (float)raw_il * volt_per_code;

    for (i = 0U; i < ANALOG_DAC_CH_COUNT; ++i)
    {
        DAC_setShadowValue(kDacBases[i], analog_dac_value(i, raw_vout, raw_il));
    }

    ++g_analog.sample_count;

    ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);
    if (ADC_getInterruptOverflowStatus(ADCA_BASE, ADC_INT_NUMBER1))
    {
        /* A sample arrived before the previous one was serviced. */
        ADC_clearInterruptOverflowStatus(ADCA_BASE, ADC_INT_NUMBER1);
        ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);
        ++g_analog.overflow_count;
    }
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP1);
}

void analog_init(void)
{
    uint32_t pwm_hz;
    uint32_t prescale;
    uint16_t tbprd = pwm_get_period_ticks();

    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_ADCA);
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_ADCB);
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_DACA);
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_DACB);
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_DACC);
    DEVICE_DELAY_US(10U);

    analog_init_dac();

    analog_init_adc_module(ADCA_BASE, ADC_CH_ADCIN2); /* ADCINA2 = Vout */
    analog_init_adc_module(ADCB_BASE, ADC_CH_ADCIN2); /* ADCINB2 = IL   */
    DEVICE_DELAY_US(1000U); /* ADC core power-up */

    /* ADCA INT1 fires at the end of SOC0 and drives the sample ISR. */
    ADC_setInterruptSource(ADCA_BASE, ADC_INT_NUMBER1, ADC_SOC_NUMBER0);
    ADC_disableContinuousMode(ADCA_BASE, ADC_INT_NUMBER1);
    ADC_enableInterrupt(ADCA_BASE, ADC_INT_NUMBER1);
    ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);
    ADC_clearInterruptOverflowStatus(ADCA_BASE, ADC_INT_NUMBER1);

    /* Divide the PWM trigger so the ISR rate stays at or below the limit. */
    if (tbprd == 0U)
    {
        tbprd = 1U;
    }
    pwm_hz = ANALOG_EPWMCLK_HZ / (2UL * (uint32_t)tbprd);
    prescale = (pwm_hz + ANALOG_MAX_SAMPLE_RATE_HZ - 1UL) /
               ANALOG_MAX_SAMPLE_RATE_HZ;
    if (prescale < 1UL)
    {
        prescale = 1UL;
    }
    if (prescale > 15UL)
    {
        prescale = 15UL;
    }
    g_analog.sample_rate_hz = pwm_hz / prescale;

    Interrupt_register(INT_ADCA1, &analog_adca1_isr);
    Interrupt_enable(INT_ADCA1);

    /* EPWM1 SOCA at CTR = 0 (middle of the EPWM1A on-time), every Nth period. */
    EPWM_disableADCTrigger(ANALOG_TRIGGER_EPWM_BASE, EPWM_SOC_A);
    EPWM_setADCTriggerSource(ANALOG_TRIGGER_EPWM_BASE, EPWM_SOC_A,
                             EPWM_SOC_TBCTR_ZERO);
    EPWM_setADCTriggerEventPrescale(ANALOG_TRIGGER_EPWM_BASE, EPWM_SOC_A,
                                    (uint16_t)prescale);
    EPWM_enableADCTrigger(ANALOG_TRIGGER_EPWM_BASE, EPWM_SOC_A);
}
