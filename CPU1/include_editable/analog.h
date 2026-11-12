#ifndef CPU1_INCLUDE_EDITABLE_ANALOG_H_
#define CPU1_INCLUDE_EDITABLE_ANALOG_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Analog I/O for the current board set (control board + Inverter_Interface_v3).
 *
 * ADC (2 channels, sampled at the same instant, triggered by the PWM):
 *   ch 0  ADCINA2  <- interface U9 (Vout differential sense, J15.4 / J15.2)
 *   ch 1  ADCINB2  <- interface U8 (IL_SENS, J15.8)
 *   Trigger = EPWM1 SOCA at CTR = 0 (middle of the EPWM1A on-time). ADCA and
 *   ADCB start on the same pulse, so both channels are simultaneous.
 *   ADCA INT1 (end of conversion) runs the sample ISR.
 *
 * DAC (3 channels):
 *   ch 0  DACA -> interface DAC1 connector   (pin shared with ADCINA0)
 *   ch 1  DACB -> interface DAC2 connector   (pin shared with ADCINA1)
 *   ch 2  DACC -> interface DAC3 connector   (pin shared with ADCINB1)
 *   Reference = VREFHI. The buffered output cannot reach the rails (keep codes
 *   roughly within 300 mV of each end for a linear output).
 */

#define ANALOG_ADC_CH_COUNT 2U
#define ANALOG_ADC_CH_VOUT 0U /* ADCINA2 */
#define ANALOG_ADC_CH_IL 1U   /* ADCINB2 */

#define ANALOG_DAC_CH_COUNT 3U

/* dac_mode[] values: what each DAC channel outputs. */
#define ANALOG_DAC_MODE_MANUAL 0U   /* dac_code[ch] (write it from easyDSP)   */
#define ANALOG_DAC_MODE_ADC_VOUT 1U /* mirror ADC ch 0 raw code (ADCINA2)     */
#define ANALOG_DAC_MODE_ADC_IL 2U   /* mirror ADC ch 1 raw code (ADCINB2)     */

/* Upper limit for the sample ISR rate. The PWM trigger is divided down
 * (1..15 PWM periods per sample) until the rate is at or below this. */
#define ANALOG_MAX_SAMPLE_RATE_HZ 50000UL

/* Live analog state. Read / write from easyDSP via g_analog. */
typedef struct
{
    uint16_t adc_raw[ANALOG_ADC_CH_COUNT]; /* latest 12-bit codes, 0..4095       */
    float adc_volt[ANALOG_ADC_CH_COUNT];   /* voltage at the MCU ADC pin [V]     */
    float vref_v;                          /* VREFHI [V] (writable; calibrate)   */
    uint32_t sample_count;                 /* ISR runs since init                */
    uint32_t overflow_count;               /* ADC interrupt overflows seen       */
    uint32_t sample_rate_hz;               /* actual sample / ISR rate           */
    uint16_t dac_mode[ANALOG_DAC_CH_COUNT]; /* ANALOG_DAC_MODE_* (writable)      */
    uint16_t dac_code[ANALOG_DAC_CH_COUNT]; /* manual code 0..4095 (writable)    */
} analog_t;

extern volatile analog_t g_analog;

/*
 * Configure ADCA/ADCB, DACA/B/C, the EPWM1 SOCA trigger and the ADCA INT1 ISR.
 * Call once, after pwm_config_init() (needs the PWM period) and after
 * Interrupt_initModule() / Interrupt_initVectorTable().
 */
void analog_init(void);

#ifdef __cplusplus
}
#endif

#endif /* CPU1_INCLUDE_EDITABLE_ANALOG_H_ */
