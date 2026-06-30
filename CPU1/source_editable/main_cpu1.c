#include "../CPU1/include_editable/initial_header.h"
#include "../CPU1/include_editable/intercore_cpu1.h"
#include "../CPU1/include_editable/pinmux.h"
#include "../CPU1/include_editable/pwm.h"

#define CPU1_HEARTBEAT_GPIO DEVICE_GPIO_PIN_LED1
#define CPU1_HEARTBEAT_GPIO_CFG DEVICE_GPIO_CFG_LED1
#define CPU1_HEARTBEAT_HALF_PERIOD_US 1000000U
#define CPU1_SUPERVISOR_STATUS_GPIO 107U
#define CPU1_SUPERVISOR_STATUS_GPIO_CFG GPIO_107_GPIO107
#define BOARD_IO_BUFFER_OE_INACTIVE 1U
#define BOARD_IO_BUFFER_OE_ACTIVE 0U
#define BOARD_IO_BUFFER_DIR_MCU_TO_BOARD 1U
#define CPU1_GPIO_DIAG_ENABLE 0U
#define CPU1_GPIO_DIAG_PIN 14U
#define CPU1_GPIO_DIAG_PIN_CFG GPIO_14_GPIO14
#define CPU1_ANALOG_TEST_ENABLE 0U
#define CPU1_COMM_TEST_ENABLE 0U
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

//
// Main
//
void main(void)
{
    Device_init();
    GPIO_setPinMuxConfig();
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
    intercore_cpu1_run_cm_handshake();
    intercore_cpu1_run_cpu2_handshake();

    for (;;)
    {
        if (g_board_io_enable_request != g_board_io_outputs_enabled)
        {
            cpu1_set_board_io_outputs(g_board_io_enable_request);
        }

        GPIO_togglePin(CPU1_HEARTBEAT_GPIO);
        DEVICE_DELAY_US(CPU1_HEARTBEAT_HALF_PERIOD_US);
    }
}

//
// End of File
//
