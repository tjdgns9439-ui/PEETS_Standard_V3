#include "../CPU1/include_editable/initial_header.h"
#include "../CPU1/include_editable/intercore_cpu1.h"
#include "../CPU1/include_editable/pinmux.h"
#include "../CPU1/include_editable/pwm.h"
#include "../CPU1/include_editable/loop_timing.h"
#include "../CPU1/include_editable/watchdog.h"
#include "easy28x_driverlib_v12.2.h" /* easyDSP real-time monitor kernel (CPU1/SCIA) */

#define CPU1_HEARTBEAT_GPIO DEVICE_GPIO_PIN_LED1
#define CPU1_HEARTBEAT_GPIO_CFG DEVICE_GPIO_CFG_LED1
#define CPU1_HEARTBEAT_HALF_PERIOD_US 1000000U
// easyDSP-writable LED command: write g_cpu1_led_command in easyDSP to drive LED1.
#define CPU1_LED_CMD_HEARTBEAT 0U  // default: slow blink (~0.5 Hz)
#define CPU1_LED_CMD_ON 1U         // solid on
#define CPU1_LED_CMD_OFF 2U        // solid off
#define CPU1_LED_CMD_BLINK_FAST 3U // fast blink (~5 Hz)
#define CPU1_SUPERVISOR_STATUS_GPIO 107U
#define CPU1_SUPERVISOR_STATUS_GPIO_CFG GPIO_107_GPIO107
#define CPU1_ENET_PHY_POWERDOWN_GPIO 108U
#define CPU1_ENET_PHY_POWERDOWN_GPIO_CFG GPIO_108_GPIO108
#define CPU1_ENET_PHY_RESET_GPIO 119U
#define CPU1_ENET_PHY_RESET_GPIO_CFG GPIO_119_GPIO119
#define BOARD_IO_BUFFER_OE_INACTIVE 1U
#define BOARD_IO_BUFFER_OE_ACTIVE 0U
#define BOARD_IO_BUFFER_DIR_MCU_TO_BOARD 1U
#define CPU1_GPIO_DIAG_ENABLE 0U
#define CPU1_GPIO_DIAG_PIN 14U
#define CPU1_GPIO_DIAG_PIN_CFG GPIO_14_GPIO14
#define CPU1_ANALOG_TEST_ENABLE 0U
#define CPU1_COMM_TEST_ENABLE 0U
#define CPU1_CM_LIVENESS_SUPERVISOR_ENABLE 1U
#define CPU1_CM_LIVENESS_STALE_LIMIT 5U
#define CPU1_CPU2_LIVENESS_SUPERVISOR_ENABLE 1U
#define CPU1_CPU2_LIVENESS_STALE_LIMIT 5U
// System-fault policy: once a fault (CPU2 dead or manual inject) is detected, all
// outputs are stopped; if it hasn't cleared within this window, reset the device.
#define CPU1_FAULT_RESET_TIMEOUT_MS 10000U
// Granularity at which the main-loop delay services the WD / LED / fault logic.
#define CPU1_WD_SERVICE_CHUNK_US 100000U
#define COMM_TEST_SCI_BASE SCIA_BASE
#define COMM_TEST_SCI_TX_PIN 34U
#define COMM_TEST_SCI_TX_PIN_CFG GPIO_34_SCIA_TX
#define COMM_TEST_SCI_RX_PIN 49U
#define COMM_TEST_SCI_RX_PIN_CFG GPIO_49_SCIA_RX
#define COMM_TEST_SCI_BAUDRATE 115200U
#define COMM_TEST_TX_BYTE 0x55U
#define COMM_TEST_TX_DELAY_US 1000U
#define ANALOG_TEST_ADC_BASE ADCA_BASE
#define ANALOG_TEST_ADC_RESULT_BASE ADCARESULT_BASE
#define ANALOG_TEST_ADC_SOC ADC_SOC_NUMBER0
#define ANALOG_TEST_ADC_INT ADC_INT_NUMBER1
#define ANALOG_TEST_ADC_CHANNEL ADC_CH_ADCIN0
#define ANALOG_TEST_ADC_ACQPS 64U
#define ANALOG_TEST_DAC_BASE DACA_BASE
#define ANALOG_TEST_DAC_STEP 64U
#define ANALOG_TEST_LOOP_DELAY_US 1000U
#define ANALOG_TEST_DAC_MAX_CODE 4095U

static volatile uint16_t g_pwm_smoke_value = 0U;
volatile uint32_t g_board_io_enable_request = 0U;
volatile uint32_t g_board_io_outputs_enabled = 0U;
volatile uint32_t g_epwm_smoke_initialized = 0U;
volatile uint16_t g_analog_test_dac_code = 0U;
volatile uint16_t g_analog_test_adc_latest = 0U;
volatile uint16_t g_analog_test_adc_min = ANALOG_TEST_DAC_MAX_CODE;
volatile uint16_t g_analog_test_adc_max = 0U;
volatile uint32_t g_analog_test_sample_count = 0U;
volatile uint32_t g_comm_test_tx_count = 0U;
volatile uint32_t g_comm_test_rx_count = 0U;
volatile uint32_t g_comm_test_error_count = 0U;
volatile uint32_t g_comm_test_rx_lastest = 0U;
volatile uint32_t g_cpu1_enet_phy_control_stage = 0U;
volatile uint32_t g_cpu1_cm_liveness_last = 0U;
volatile uint32_t g_cpu1_cm_liveness_stale_count = 0U;
volatile uint32_t g_cpu1_cm_recovery_count = 0U;
volatile uint32_t g_cpu1_cm_recovery_stage = 0U;
volatile uint32_t g_cpu1_cpu2_liveness_last = 0U;
volatile uint32_t g_cpu1_cpu2_liveness_stale_count = 0U;
volatile uint32_t g_cpu1_cpu2_recovery_count = 0U;
volatile uint32_t g_cpu1_cpu2_recovery_stage = 0U;
volatile uint32_t g_cpu1_led_command = CPU1_LED_CMD_HEARTBEAT; // easyDSP-writable
volatile uint32_t g_cpu1_cpu2_fault_latched = 0U; // last CPU2 fault CPU1 acted on
volatile uint32_t g_cpu1_fault_inject = 0U;    // easyDSP-writable manual fault trigger
volatile uint32_t g_cpu1_cpu2_dead = 0U;       // 1 = CPU2 not responding
volatile uint32_t g_cpu1_system_fault = 0U;    // 1 = system in FAULT (safe) state
volatile uint32_t g_cpu1_fault_elapsed_ms = 0U; // time spent in FAULT state

static const uint32_t kBufferOeGpios[] = {
    42U, 43U, 46U, 50U, 100U};

static const uint32_t kBufferDirGpios[] = {
    58U, 59U, 120U};

static void cpu1_init_heartbeat(void)
{
    GPIO_setPinConfig(CPU1_HEARTBEAT_GPIO_CFG);
    GPIO_setDirectionMode(CPU1_HEARTBEAT_GPIO, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(CPU1_HEARTBEAT_GPIO, GPIO_PIN_TYPE_STD);
    GPIO_writePin(CPU1_HEARTBEAT_GPIO, 0U);
}

static void cpu1_init_board_io_safe(void)
{
    uint16_t i;

    for (i = 0U; i < (sizeof(kBufferOeGpios) / sizeof(kBufferOeGpios[0])); ++i)
    {
        GPIO_setDirectionMode(kBufferOeGpios[i], GPIO_DIR_MODE_OUT);
        GPIO_setPadConfig(kBufferOeGpios[i], GPIO_PIN_TYPE_STD);
        GPIO_writePin(kBufferOeGpios[i], BOARD_IO_BUFFER_OE_INACTIVE);
    }

    for (i = 0U; i < (sizeof(kBufferDirGpios) / sizeof(kBufferDirGpios[0])); ++i)
    {
        GPIO_setDirectionMode(kBufferDirGpios[i], GPIO_DIR_MODE_OUT);
        GPIO_setPadConfig(kBufferDirGpios[i], GPIO_PIN_TYPE_STD);
        GPIO_writePin(kBufferDirGpios[i], BOARD_IO_BUFFER_DIR_MCU_TO_BOARD);
    }

    g_board_io_outputs_enabled = 0U;
}

static void cpu1_init_supervisor_status_input(void)
{
    GPIO_setPinConfig(CPU1_SUPERVISOR_STATUS_GPIO_CFG);
    GPIO_setDirectionMode(CPU1_SUPERVISOR_STATUS_GPIO, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(CPU1_SUPERVISOR_STATUS_GPIO, GPIO_PIN_TYPE_STD);
}

static void cpu1_init_ethernet_phy_control(void)
{
    g_cpu1_enet_phy_control_stage = 1U;
    SysCtl_setEnetClk(SYSCTL_ENETCLKOUT_DIV_2, SYSCTL_SOURCE_SYSPLL);

    GPIO_setPinConfig(CPU1_ENET_PHY_POWERDOWN_GPIO_CFG);
    GPIO_setDirectionMode(CPU1_ENET_PHY_POWERDOWN_GPIO, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(CPU1_ENET_PHY_POWERDOWN_GPIO, GPIO_PIN_TYPE_PULLUP);
    GPIO_writePin(CPU1_ENET_PHY_POWERDOWN_GPIO, 1U);
    g_cpu1_enet_phy_control_stage = 2U;

    GPIO_setPinConfig(CPU1_ENET_PHY_RESET_GPIO_CFG);
    GPIO_setDirectionMode(CPU1_ENET_PHY_RESET_GPIO, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(CPU1_ENET_PHY_RESET_GPIO, GPIO_PIN_TYPE_PULLUP);
    GPIO_writePin(CPU1_ENET_PHY_RESET_GPIO, 1U);
    g_cpu1_enet_phy_control_stage = 3U;
}

static void cpu1_configure_ethernet_mii_pins(void)
{
    GPIO_setPinConfig(GPIO_105_ENET_MDIO_CLK);
    GPIO_setPinConfig(GPIO_106_ENET_MDIO_DATA);

    GPIO_setPinConfig(GPIO_109_ENET_MII_CRS);
    GPIO_setPinConfig(GPIO_110_ENET_MII_COL);

    GPIO_setPinConfig(GPIO_75_ENET_MII_TX_DATA0);
    GPIO_setPinConfig(GPIO_122_ENET_MII_TX_DATA1);
    GPIO_setPinConfig(GPIO_123_ENET_MII_TX_DATA2);
    GPIO_setPinConfig(GPIO_124_ENET_MII_TX_DATA3);
    GPIO_setPinConfig(GPIO_118_ENET_MII_TX_EN);

    GPIO_setPinConfig(GPIO_114_ENET_MII_RX_DATA0);
    GPIO_setPinConfig(GPIO_115_ENET_MII_RX_DATA1);
    GPIO_setPinConfig(GPIO_116_ENET_MII_RX_DATA2);
    GPIO_setPinConfig(GPIO_117_ENET_MII_RX_DATA3);
    GPIO_setPinConfig(GPIO_113_ENET_MII_RX_ERR);
    GPIO_setPinConfig(GPIO_112_ENET_MII_RX_DV);

    GPIO_setPinConfig(GPIO_44_ENET_MII_TX_CLK);
    GPIO_setPinConfig(GPIO_111_ENET_MII_RX_CLK);
}

static void cpu1_run_single_gpio_diag(void)
{
    GPIO_setPinConfig(CPU1_GPIO_DIAG_PIN_CFG);
    GPIO_setDirectionMode(CPU1_GPIO_DIAG_PIN, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(CPU1_GPIO_DIAG_PIN, GPIO_PIN_TYPE_STD);

    for (;;)
    {
        GPIO_writePin(CPU1_GPIO_DIAG_PIN, 1U);
        DEVICE_DELAY_US(1000000U);
        GPIO_writePin(CPU1_GPIO_DIAG_PIN, 0U);
        DEVICE_DELAY_US(1000000U);
    }
}

static void analog_test_init_dac(void)
{
    DAC_setReferenceVoltage(ANALOG_TEST_DAC_BASE, DAC_REF_ADC_VREFHI);
    DAC_setLoadMode(ANALOG_TEST_DAC_BASE, DAC_LOAD_SYSCLK);
    DAC_setShadowValue(ANALOG_TEST_DAC_BASE, 0U);
    DAC_enableOutput(ANALOG_TEST_DAC_BASE);
    DEVICE_DELAY_US(10U);
}

static void analog_test_init_adc(void)
{
    ADC_setPrescaler(ANALOG_TEST_ADC_BASE, ADC_CLK_DIV_4_0);
    ADC_setMode(ANALOG_TEST_ADC_BASE, ADC_RESOLUTION_12BIT,
                ADC_MODE_SINGLE_ENDED);
    ADC_setInterruptPulseMode(ANALOG_TEST_ADC_BASE, ADC_PULSE_END_OF_CONV);
    ADC_enableConverter(ANALOG_TEST_ADC_BASE);
    DEVICE_DELAY_US(1000U);

    ADC_setupSOC(ANALOG_TEST_ADC_BASE, ANALOG_TEST_ADC_SOC,
                 ADC_TRIGGER_SW_ONLY, ANALOG_TEST_ADC_CHANNEL,
                 ANALOG_TEST_ADC_ACQPS);
    ADC_setInterruptSource(ANALOG_TEST_ADC_BASE, ANALOG_TEST_ADC_INT,
                           ANALOG_TEST_ADC_SOC);
    ADC_enableInterrupt(ANALOG_TEST_ADC_BASE, ANALOG_TEST_ADC_INT);
    ADC_clearInterruptStatus(ANALOG_TEST_ADC_BASE, ANALOG_TEST_ADC_INT);
}

static uint16_t analog_test_sample_adc(void)
{
    ADC_forceSOC(ANALOG_TEST_ADC_BASE, ANALOG_TEST_ADC_SOC);

    while (!ADC_getInterruptStatus(ANALOG_TEST_ADC_BASE, ANALOG_TEST_ADC_INT))
    {
    }

    ADC_clearInterruptStatus(ANALOG_TEST_ADC_BASE, ANALOG_TEST_ADC_INT);

    return ADC_readResult(ANALOG_TEST_ADC_RESULT_BASE, ANALOG_TEST_ADC_SOC);
}

static void analog_test_run(void)
{
    uint16_t dacCode = 0U;
    uint16_t adcCode;

    analog_test_init_dac();
    analog_test_init_adc();

    for (;;)
    {
        DAC_setShadowValue(ANALOG_TEST_DAC_BASE, dacCode);
        DEVICE_DELAY_US(ANALOG_TEST_LOOP_DELAY_US);

        adcCode = analog_test_sample_adc();

        g_analog_test_dac_code = dacCode;
        g_analog_test_adc_latest = adcCode;
        if (adcCode < g_analog_test_adc_min)
        {
            g_analog_test_adc_min = adcCode;
        }
        if (adcCode > g_analog_test_adc_max)
        {
            g_analog_test_adc_max = adcCode;
        }
        ++g_analog_test_sample_count;

        dacCode = (uint16_t)((dacCode + ANALOG_TEST_DAC_STEP) &
                             ANALOG_TEST_DAC_MAX_CODE);
    }
}

static void comm_test_init_scia(void)
{
    GPIO_setPinConfig(COMM_TEST_SCI_TX_PIN_CFG);
    GPIO_setPinConfig(COMM_TEST_SCI_RX_PIN_CFG);
    GPIO_setDirectionMode(COMM_TEST_SCI_TX_PIN, GPIO_DIR_MODE_OUT);
    GPIO_setDirectionMode(COMM_TEST_SCI_RX_PIN, GPIO_DIR_MODE_IN);
    GPIO_setPadConfig(COMM_TEST_SCI_TX_PIN, GPIO_PIN_TYPE_STD);
    GPIO_setPadConfig(COMM_TEST_SCI_RX_PIN, GPIO_PIN_TYPE_STD);
    GPIO_setQualificationMode(COMM_TEST_SCI_RX_PIN, GPIO_QUAL_ASYNC);

    SCI_performSoftwareReset(COMM_TEST_SCI_BASE);
    SCI_setConfig(COMM_TEST_SCI_BASE, DEVICE_LSPCLK_FREQ,
                  COMM_TEST_SCI_BAUDRATE,
                  SCI_CONFIG_WLEN_8 | SCI_CONFIG_STOP_ONE |
                      SCI_CONFIG_PAR_NONE);
    SCI_resetChannels(COMM_TEST_SCI_BASE);
    SCI_resetRxFIFO(COMM_TEST_SCI_BASE);
    SCI_resetTxFIFO(COMM_TEST_SCI_BASE);
    SCI_enableFIFO(COMM_TEST_SCI_BASE);
    SCI_enableModule(COMM_TEST_SCI_BASE);
}

static void comm_test_run_scia_tx(void)
{
    comm_test_init_scia();

    for (;;)
    {
        SCI_writeCharBlockingFIFO(COMM_TEST_SCI_BASE, COMM_TEST_TX_BYTE);
        ++g_comm_test_tx_count;

        if (SCI_getRxFIFOStatus(COMM_TEST_SCI_BASE) != SCI_FIFO_RX0)
        {
            g_comm_test_rx_lastest =
                SCI_readCharBlockingFIFO(COMM_TEST_SCI_BASE);
            ++g_comm_test_rx_count;

            if (g_comm_test_rx_lastest != COMM_TEST_TX_BYTE)
            {
                ++g_comm_test_error_count;
            }
        }

        DEVICE_DELAY_US(COMM_TEST_TX_DELAY_US);
    }
}

static void cpu1_set_board_io_outputs(uint32_t enable)
{
    uint16_t i;

    if (enable == 0U)
    {
        for (i = 0U; i < (sizeof(kBufferOeGpios) / sizeof(kBufferOeGpios[0])); ++i)
        {
            GPIO_writePin(kBufferOeGpios[i], BOARD_IO_BUFFER_OE_INACTIVE);
        }

        pwm_smoke_force_safe_off();
        g_board_io_outputs_enabled = 0U;
        return;
    }

    pwm_smoke_release_outputs();

    for (i = 0U; i < (sizeof(kBufferOeGpios) / sizeof(kBufferOeGpios[0])); ++i)
    {
        GPIO_writePin(kBufferOeGpios[i], BOARD_IO_BUFFER_OE_ACTIVE);
    }

    g_board_io_outputs_enabled = 1U;
}

static void cpu1_service_cm_liveness_supervisor(void)
{
#if CPU1_CM_LIVENESS_SUPERVISOR_ENABLE
    uint32_t cm_liveness = g_cm_to_cpu1_mailbox;

    if(cm_liveness != g_cpu1_cm_liveness_last)
    {
        g_cpu1_cm_liveness_last = cm_liveness;
        g_cpu1_cm_liveness_stale_count = 0U;
        return;
    }

    if(g_cm_handshake_status != CM_HANDSHAKE_STATUS_OK)
    {
        return;
    }

    if(g_cpu1_cm_liveness_stale_count < CPU1_CM_LIVENESS_STALE_LIMIT)
    {
        ++g_cpu1_cm_liveness_stale_count;
        return;
    }

    g_cpu1_cm_recovery_stage = 1U;
    SysCtl_controlCMReset(SYSCTL_CORE_ACTIVE);
    DEVICE_DELAY_US(1000U);

    g_cpu1_cm_recovery_stage = 2U;
    intercore_cpu1_run_cm_handshake();

    ++g_cpu1_cm_recovery_count;
    g_cpu1_cm_liveness_last = g_cm_to_cpu1_mailbox;
    g_cpu1_cm_liveness_stale_count = 0U;
    g_cpu1_cm_recovery_stage = 3U;
#endif
}

static void cpu1_service_cpu2_liveness_supervisor(void)
{
#if CPU1_CPU2_LIVENESS_SUPERVISOR_ENABLE
    uint32_t cpu2_liveness = g_cpu2_monitor.mbox;

    if(cpu2_liveness != g_cpu1_cpu2_liveness_last)
    {
        g_cpu1_cpu2_liveness_last = cpu2_liveness;
        g_cpu1_cpu2_liveness_stale_count = 0U;
        g_cpu1_cpu2_dead = 0U; // CPU2 responded -> alive
        return;
    }

    if(g_cpu2_handshake_status != CPU2_HANDSHAKE_STATUS_OK)
    {
        return;
    }

    if(g_cpu1_cpu2_liveness_stale_count < CPU1_CPU2_LIVENESS_STALE_LIMIT)
    {
        ++g_cpu1_cpu2_liveness_stale_count;
        return;
    }

    g_cpu1_cpu2_dead = 1U; // CPU2 silent past the stale limit -> considered dead
    g_cpu1_cpu2_recovery_stage = 1U;
    SysCtl_controlCPU2Reset(SYSCTL_CORE_ACTIVE);
    DEVICE_DELAY_US(1000U);

    g_cpu1_cpu2_recovery_stage = 2U;
    intercore_cpu1_run_cpu2_handshake();

    ++g_cpu1_cpu2_recovery_count;
    g_cpu1_cpu2_liveness_last = g_cpu2_monitor.mbox;
    g_cpu1_cpu2_liveness_stale_count = 0U;
    g_cpu1_cpu2_recovery_stage = 3U;
#endif
}

//
// CPU2 fault auto-response. If CPU2 reports a fault (e.g. e-stop) in its
// CPU2->CPU1 monitor block, CPU1 forces the board outputs to the safe (off)
// state and stops the loop from re-enabling them. Serviced every ~100 ms.
//
static void cpu1_service_cpu2_fault(void)
{
    if (g_cpu2_handshake_status != CPU2_HANDSHAKE_STATUS_OK)
    {
        return; // CPU2 not up -> its monitor block isn't trustworthy yet
    }

    if (g_cpu2_monitor.fault_code != CPU2_FAULT_NONE)
    {
        if (g_board_io_outputs_enabled != 0U)
        {
            cpu1_set_board_io_outputs(0U);
        }
        g_board_io_enable_request = 0U; // don't let the loop re-enable while faulted
        g_cpu1_cpu2_fault_latched = g_cpu2_monitor.fault_code;
    }
}

//
// Safe state: stop all board outputs / PWM and keep them stopped.
//
static void cpu1_enter_safe_state(void)
{
    g_board_io_enable_request = 0U; // don't let anything re-enable while faulted
    if (g_board_io_outputs_enabled != 0U)
    {
        cpu1_set_board_io_outputs(0U); // also forces PWM safe-off
    }
    pwm_smoke_force_safe_off();
}

//
// System-fault supervisor (serviced every ~100 ms). A fault = CPU2 not responding
// OR a manual inject (g_cpu1_fault_inject). While faulted, all outputs are held
// off; if the fault does not clear within CPU1_FAULT_RESET_TIMEOUT_MS, the whole
// device is reset. Recovers automatically if the condition clears in time.
//
static void cpu1_service_system_fault(void)
{
    // Wall-clock elapsed via the free-running CPU Timer 0 (started by
    // loop_timing_init), so blocking work (e.g. a CPU2 recovery handshake) still
    // counts toward the timeout. Down-counter: delta = last - now (mod 2^32).
    static uint32_t s_last_count = 0U;
    static uint16_t s_count_valid = 0U;
    uint32_t now = CPUTimer_getTimerCount(CPUTIMER0_BASE);
    uint32_t fault = (g_cpu1_cpu2_dead != 0U) || (g_cpu1_fault_inject != 0U);

    if (fault)
    {
        if (g_cpu1_system_fault == 0U)
        {
            g_cpu1_system_fault = 1U; // entering FAULT
            g_cpu1_fault_elapsed_ms = 0U;
        }
        else if (s_count_valid != 0U)
        {
            uint32_t delta = (uint32_t)(s_last_count - now);
            g_cpu1_fault_elapsed_ms += delta / (DEVICE_SYSCLK_FREQ / 1000U);
            if (g_cpu1_fault_elapsed_ms >= CPU1_FAULT_RESET_TIMEOUT_MS)
            {
                watchdog_force_reset(); // does not return
            }
        }
        cpu1_enter_safe_state(); // keep everything safe-off every cycle
    }
    else if (g_cpu1_system_fault != 0U)
    {
        g_cpu1_system_fault = 0U; // cleared within the grace window -> resume
        g_cpu1_fault_elapsed_ms = 0U;
    }

    s_last_count = now;
    s_count_valid = 1U;
}

//
// LED command handler. easyDSP (or any code) writes g_cpu1_led_command; this is
// serviced every ~100 ms so LED1 reacts within 100 ms. Default keeps the
// original heartbeat blink. A system fault overrides it with a fast blink.
//
static void cpu1_service_led(void)
{
    static uint32_t led_tick = 0U;

    ++led_tick;

    if (g_cpu1_system_fault != 0U)
    {
        GPIO_togglePin(CPU1_HEARTBEAT_GPIO); // FAULT indication: fast blink
        return;
    }

    switch (g_cpu1_led_command)
    {
    case CPU1_LED_CMD_ON:
        GPIO_writePin(CPU1_HEARTBEAT_GPIO, 0U); // LED1 is active-low: 0 = on
        break;
    case CPU1_LED_CMD_OFF:
        GPIO_writePin(CPU1_HEARTBEAT_GPIO, 1U); // 1 = off
        break;
    case CPU1_LED_CMD_BLINK_FAST:
        GPIO_togglePin(CPU1_HEARTBEAT_GPIO); // toggles every ~100 ms => ~5 Hz
        break;
    case CPU1_LED_CMD_HEARTBEAT:
    default:
        if ((led_tick % 10U) == 0U) // 10 x 100 ms => ~1 s toggle (~0.5 Hz)
        {
            GPIO_togglePin(CPU1_HEARTBEAT_GPIO);
        }
        break;
    }
}

//
// Blocking delay that services the watchdog and the LED command every
// CPU1_WD_SERVICE_CHUNK_US so a long wait (the 1 s heartbeat delay) never trips
// the ~0.84 s watchdog and the LED still reacts to commands within ~100 ms.
//
static void cpu1_delay_us_serviced(uint32_t total_us)
{
    uint32_t remaining = total_us;

    while (remaining > 0U)
    {
        uint32_t chunk = (remaining > CPU1_WD_SERVICE_CHUNK_US)
                             ? CPU1_WD_SERVICE_CHUNK_US
                             : remaining;
        watchdog_service();
        DEVICE_DELAY_US(chunk);
        cpu1_service_system_fault(); // may not return (device reset after timeout)
        cpu1_service_led();
        cpu1_service_cpu2_fault();
        remaining -= chunk;
    }
    watchdog_service();
}

//
// Main
//
void main(void)
{
    Device_init();
    GPIO_setPinMuxConfig();
    cpu1_configure_ethernet_mii_pins();
    cpu1_init_ethernet_phy_control();
    g_pwm_smoke_value = pwm_test_run(1U);
    cpu1_init_board_io_safe();
    cpu1_init_supervisor_status_input();
    cpu1_init_heartbeat();

    if (CPU1_GPIO_DIAG_ENABLE != 0U)
    {
        cpu1_run_single_gpio_diag();
    }

    if (CPU1_ANALOG_TEST_ENABLE != 0U)
    {
        analog_test_run();
    }

    if (CPU1_COMM_TEST_ENABLE != 0U)
    {
        comm_test_run_scia_tx();
    }

    pwm_smoke_init();
    pwm_smoke_force_safe_off();
    g_epwm_smoke_initialized = 1U;

    /*
     * easyDSP real-time monitor (CPU1 SCI-A @ GPIO29 TX / GPIO28 RX; CPU2 SCI-B
     * @ GPIO14 TX / GPIO15 RX, 115200). New board routes easyDSP headers to the
     * ROM SCI-boot pins so download + monitoring both work.
     * Initialized BEFORE the intercore handshakes so the SCI agent starts even
     * when CM/CPU2 are not loaded. Otherwise the handshakes below block here and
     * easyDSP_SCI_Init() is never reached, so the agent never runs and easyDSP
     * shows "?". It registers the SCIA RX ISR and enables EINT/ERTM internally.
     * Keep CPU1_COMM_TEST_ENABLE = 0 so SCIA is free.
     */
    /*
     * Release GPIO49/GPIO34 from SCIA_RX/SCIA_TX. The sysconfig GPIO_setPinMuxConfig()
     * above still maps SCI-A to the OLD board pins (GPIO49 = SCIA_RX, GPIO34 = SCIA_TX).
     * easyDSP_SCI_Init() below maps SCI-A to the NEW board pins (GPIO28 RX / GPIO29 TX).
     * Leaving GPIO49 as a *second* SCIA_RX pin ties the SCI-A RX input net to that
     * idle-high, unconnected pin, which masks easyDSP's data on GPIO28 -> the kernel
     * never receives and easyDSP shows "?". Revert them to plain GPIO so only GPIO28/29
     * drive SCI-A.
     */
    GPIO_setPinConfig(GPIO_49_GPIO49);
    GPIO_setPinConfig(GPIO_34_GPIO34);

    Interrupt_initModule();
    Interrupt_initVectorTable();
    easyDSP_SCI_Init();

    /*
     * CPU2/CM are booted inside the handshake routines below (Device_bootCPU2/
     * bootCM for FLASH; skipped for RAM builds where the debugger boots them).
     */
    intercore_cpu1_run_cm_handshake();
    intercore_cpu1_run_cpu2_handshake();

    loop_timing_init();
    // Arm the watchdog only after the (up-to-5 s) boot handshakes so they can't
    // trip it. From here a code hang resets CPU1 within ~0.84 s.
    watchdog_init();


    for (;;)
    {
        loop_timing_loop_begin();

        if (g_board_io_enable_request != g_board_io_outputs_enabled)
        {
            cpu1_set_board_io_outputs(g_board_io_enable_request);
        }

        cpu1_service_cm_liveness_supervisor();
        cpu1_service_cpu2_liveness_supervisor();

        // Work done for this iteration; the delay below is the loop's idle time.
        loop_timing_work_end();
        cpu1_delay_us_serviced(CPU1_HEARTBEAT_HALF_PERIOD_US);
    }
}

//
// End of File
//
