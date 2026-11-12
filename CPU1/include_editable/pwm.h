#ifndef PWM_H_
#define PWM_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// channel_sel bitmask: which EPWM module(s) (= half-bridge legs) are driven.
#define PWM_CH_EPWM1 0x0001U
#define PWM_CH_EPWM2 0x0002U
#define PWM_CH_EPWM3 0x0004U
#define PWM_CH_EPWM4 0x0008U

// Full-bridge grouping. One full bridge = two legs = two EPWM modules.
//   Bridge 1 = EPWM1 (leg A) + EPWM2 (leg B)  -> interface PWM1A/1B + PWM2A/2B
//   Bridge 2 = EPWM3 (leg A) + EPWM4 (leg B)  -> interface PWM3A/3B + PWM4A/4B
#define PWM_BRIDGE1 (PWM_CH_EPWM1 | PWM_CH_EPWM2)
#define PWM_BRIDGE2 (PWM_CH_EPWM3 | PWM_CH_EPWM4)

#define PWM_LEG_COUNT 4U // leg index 0..3 = EPWM1..EPWM4

// PWM control/config. Writable live (e.g. from easyDSP) via g_pwm_config.
typedef struct
{
    uint32_t enable;          // PWM 활성여부: 0 = off, 1 = on
    uint32_t freq_hz;         // PWM 주파수 [Hz] (init 시에만 적용)
    uint32_t deadtime_on_ns;  // ON  dead-time  (rising edge / RED) [ns]
    uint32_t deadtime_off_ns; // OFF dead-time (falling edge / FED) [ns]
    uint32_t channel_sel;     // 구동할 레그: PWM_CH_* / PWM_BRIDGE* bitmask
    // 레그별 듀티 (상측 스위치 ON 비율), 0.0 .. 1.0. [0]=EPWM1 .. [3]=EPWM4
    float duty[PWM_LEG_COUNT];
    // 레그별 위상 [deg], EPWM1 기준. +는 지연(lag), -는 앞섬(lead), -180 .. +180.
    // [0](EPWM1)은 기준이라 무시된다.
    float phase_deg[PWM_LEG_COUNT];
} pwm_config_t;

extern volatile pwm_config_t g_pwm_config;

// Why the PWM is not reaching the power stage. Bitmask; 0 = nothing blocks it.
#define PWM_OFF_DISABLED 0x0001U     // g_pwm_config.enable = 0
#define PWM_OFF_NO_CHANNEL 0x0002U   // channel_sel selects no leg
#define PWM_OFF_SYSTEM_FAULT 0x0004U // CPU1 system fault (CPU2 dead / fault inject)
#define PWM_OFF_CPU2_FAULT 0x0008U   // CPU2 reported a fault (e.g. e-stop)
#define PWM_OFF_BUFFER 0x0010U       // board buffers off (g_board_io_enable_request = 0)
// Reasons that force the legs off inside the MCU (trip). PWM_OFF_BUFFER is not
// one of them: with the buffers off the MCU pins may still switch, but nothing
// reaches the board.
#define PWM_OFF_FAULT_MASK (PWM_OFF_SYSTEM_FAULT | PWM_OFF_CPU2_FAULT)

// Live PWM status. Read from easyDSP via g_pwm_status to see at a glance
// whether the PWM is running and, if not, why.
typedef struct
{
    uint16_t leg_switching[PWM_LEG_COUNT]; // 1 = leg is switching at the MCU pin
                                           //     (read back from the trip flag)
    uint16_t output_at_board; // 1 = at least one leg switching AND buffers on
    uint16_t off_reason;      // PWM_OFF_* bits that apply right now (0 = none)
    uint16_t fault_latched;   // PWM_OFF_*_FAULT bits seen since last cleared;
                              //   stays set after the fault goes away.
                              //   Write 0 from easyDSP to clear.
} pwm_status_t;

extern volatile pwm_status_t g_pwm_status;

uint16_t pwm_test_run(uint16_t seed);
void pwm_smoke_init(void);
void pwm_smoke_force_safe_off(void);
void pwm_smoke_release_outputs(void);

// Two full bridges on EPWM1..4 driven by g_pwm_config.
//   EPWMxA = high side, EPWMxB = low side (complementary + dead-band).
//   Center-aligned (up-down). EPWM1 is the time-base master; EPWM2..4 are
//   synchronized to it and shifted by phase_deg[].
//
//   Typical settings:
//     phase-shift full bridge / DAB : duty = 0.5 on all legs, control phase_deg[]
//       phase_deg[1] = bridge-1 inner phase (180 = full square wave)
//       phase_deg[2] = bridge-2 shift against bridge 1 (outer phase)
//       phase_deg[3] = phase_deg[2] + bridge-2 inner phase
//     unipolar PWM (inverter)       : phase 0, duty[legB] = 1 - duty[legA]
//
// pwm_config_init(): configure the modules and apply g_pwm_config INCLUDING the
//   frequency. Call once (after pwm_smoke_force_safe_off()). Frequency is applied
//   ONLY here — changing freq_hz while running is unsafe and is ignored at runtime.
// pwm_config_service(): apply the live fields (duty, phase, dead-times, enable,
//   channel_sel) and refresh g_pwm_status. block_reasons = PWM_OFF_* bits known
//   to the caller (faults, buffer state); any PWM_OFF_FAULT_MASK bit forces all
//   legs OFF. Pass 0 when nothing blocks. Call periodically.
// pwm_get_period_ticks(): TBPRD in use (for ADC trigger / monitoring).
void pwm_config_init(void);
void pwm_config_service(uint16_t block_reasons);
uint16_t pwm_get_period_ticks(void);

#ifdef __cplusplus
}
#endif

#endif
