/**
 * @file valve_task.c
 * @brief Valve control task implementation
 */

#include "sdk_config.h"
#include "FreeRTOS.h"
#include "task.h"

#include "nrf_gpio.h"
#include "nrfx_saadc.h"
#include "SEGGER_RTT.h"

#include "valve_task.h"
#include "board_config.h"

/* Helper macros */
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/* ==========================================================================
 * PRIVATE DATA
 * ========================================================================== */

static valve_state_t m_state = VALVE_STATE_IDLE;
static uint8_t m_status_flags = 0;
static uint16_t m_current_ma = 0;
static TickType_t m_operation_start = 0;

/* Command queue */
typedef enum {
    VALVE_CMD_NONE,
    VALVE_CMD_OPEN,
    VALVE_CMD_CLOSE,
    VALVE_CMD_STOP,
    VALVE_CMD_EMERGENCY_CLOSE
} valve_cmd_t;

static volatile valve_cmd_t m_pending_cmd = VALVE_CMD_NONE;

/* ==========================================================================
 * DRV8876 H-BRIDGE CONTROL
 * Truth table:
 *   IN1=L, IN2=L: Coast (Hi-Z outputs)
 *   IN1=L, IN2=H: Reverse (close valve)
 *   IN1=H, IN2=L: Forward (open valve)
 *   IN1=H, IN2=H: Brake (low-side on)
 * ========================================================================== */

static bool m_drv_fault = false;

static void drv8876_init(void)
{
    /* Configure DRV8876 control pins as outputs */
    nrf_gpio_cfg_output(DRV_IN1_PIN);
    nrf_gpio_cfg_output(DRV_IN2_PIN);
    nrf_gpio_cfg_output(DRV_nSLEEP_PIN);
    
    /* Configure fault pin as input with pullup (open drain output from DRV8876) */
    nrf_gpio_cfg_input(DRV_nFAULT_PIN, NRF_GPIO_PIN_PULLUP);

    /* Start in coast mode (both inputs low) */
    nrf_gpio_pin_clear(DRV_IN1_PIN);
    nrf_gpio_pin_clear(DRV_IN2_PIN);
    
    /* Wake up the driver (nSLEEP is active low) */
    nrf_gpio_pin_set(DRV_nSLEEP_PIN);

    SEGGER_RTT_printf(0, "DRV8876 initialized\n");
}

static void drv8876_open(void)
{
    /* Forward: IN1=H, IN2=L */
    nrf_gpio_pin_clear(DRV_IN2_PIN);
    nrf_gpio_pin_set(DRV_IN1_PIN);
}

static void drv8876_close(void)
{
    /* Reverse: IN1=L, IN2=H */
    nrf_gpio_pin_clear(DRV_IN1_PIN);
    nrf_gpio_pin_set(DRV_IN2_PIN);
}

static void drv8876_stop(void)
{
    /* Coast: IN1=L, IN2=L (Hi-Z outputs) */
    nrf_gpio_pin_clear(DRV_IN1_PIN);
    nrf_gpio_pin_clear(DRV_IN2_PIN);
}

static void drv8876_brake(void)
{
    /* Brake: IN1=H, IN2=H (low-side on, motor shorted) */
    nrf_gpio_pin_set(DRV_IN1_PIN);
    nrf_gpio_pin_set(DRV_IN2_PIN);
}

static void drv8876_sleep(void)
{
    /* Enter sleep mode for low power */
    drv8876_stop();
    nrf_gpio_pin_clear(DRV_nSLEEP_PIN);
}

static void drv8876_wake(void)
{
    /* Exit sleep mode */
    nrf_gpio_pin_set(DRV_nSLEEP_PIN);
    /* Wait for driver to wake up (tSLEEP = 1ms typical) */
    vTaskDelay(pdMS_TO_TICKS(2));
}

static bool drv8876_check_fault(void)
{
    /* nFAULT is active low */
    m_drv_fault = (nrf_gpio_pin_read(DRV_nFAULT_PIN) == 0);
    return m_drv_fault;
}

/* ==========================================================================
 * CURRENT SENSING
 * ========================================================================== */

static void adc_init(void)
{
    nrfx_saadc_config_t saadc_config = NRFX_SAADC_DEFAULT_CONFIG;
    nrfx_saadc_init(&saadc_config, NULL);

    nrf_saadc_channel_config_t channel_config = NRFX_SAADC_DEFAULT_CHANNEL_CONFIG_SE(
        NRF_SAADC_INPUT_AIN0  /* P0.02 */
    );
    channel_config.gain = NRF_SAADC_GAIN1_4;
    channel_config.reference = NRF_SAADC_REFERENCE_VDD4;

    nrfx_saadc_channel_init(0, &channel_config);

    SEGGER_RTT_printf(0, "ADC initialized\n");
}

static uint16_t read_current_ma(void)
{
    nrf_saadc_value_t sample;
    nrfx_saadc_sample_convert(0, &sample);

    if (sample < 0) sample = 0;

    /* Convert to mA using DRV8876 IPROPI output
     * IPROPI outputs 1.2mA per amp of motor current
     * With R32=1K sense resistor: V_IPROPI = 1.2V per amp
     * 
     * ADC config: Vref = VDD/4 = 0.825V, Gain = 1/4, so full scale = 3.3V
     * Resolution = 10 bits (0-1023)
     * 
     * V_measured = sample * 3.3 / 1024
     * I_motor = V_measured / 1.2 (amps)
     * I_mA = V_measured / 1.2 * 1000 = sample * 3.3 * 1000 / (1024 * 1.2)
     *      = sample * 3300 / 1228.8 = sample * 2.686
     */
    uint32_t current_ma = ((uint32_t)sample * 2686) / 1000;
    
    return (uint16_t)MIN(current_ma, 0xFFFF);
}

/* ==========================================================================
 * LIMIT SWITCHES
 * ========================================================================== */

bool valve_is_open(void)
{
    return (nrf_gpio_pin_read(LIMIT_OPEN_PIN) == 0);
}

bool valve_is_closed(void)
{
    return (nrf_gpio_pin_read(LIMIT_CLOSED_PIN) == 0);
}

/* ==========================================================================
 * STATE MACHINE
 * ========================================================================== */

static void enter_state(valve_state_t new_state)
{
    if (m_state == new_state) return;

    SEGGER_RTT_printf(0, "Valve: %d -> %d\n", m_state, new_state);
    m_state = new_state;

    switch (new_state) {
        case VALVE_STATE_OPENING:
            m_status_flags &= ~(STATUS_FLAG_OPEN | STATUS_FLAG_CLOSED | STATUS_FLAG_FAULT |
                               STATUS_FLAG_OVERCURRENT | STATUS_FLAG_TIMEOUT);
            m_status_flags |= STATUS_FLAG_MOVING;
            m_operation_start = xTaskGetTickCount();
            drv8876_wake();
            drv8876_open();
            break;

        case VALVE_STATE_CLOSING:
            m_status_flags &= ~(STATUS_FLAG_OPEN | STATUS_FLAG_CLOSED | STATUS_FLAG_FAULT |
                               STATUS_FLAG_OVERCURRENT | STATUS_FLAG_TIMEOUT);
            m_status_flags |= STATUS_FLAG_MOVING;
            m_operation_start = xTaskGetTickCount();
            drv8876_wake();
            drv8876_close();
            break;

        case VALVE_STATE_OPEN:
            drv8876_brake();  /* Brief brake to stop momentum */
            vTaskDelay(pdMS_TO_TICKS(50));
            drv8876_stop();
            drv8876_sleep();  /* Enter low power mode */
            m_status_flags &= ~STATUS_FLAG_MOVING;
            m_status_flags |= STATUS_FLAG_OPEN;
            SEGGER_RTT_printf(0, "Valve: OPEN\n");
            break;

        case VALVE_STATE_CLOSED:
            drv8876_brake();  /* Brief brake to stop momentum */
            vTaskDelay(pdMS_TO_TICKS(50));
            drv8876_stop();
            drv8876_sleep();  /* Enter low power mode */
            m_status_flags &= ~STATUS_FLAG_MOVING;
            m_status_flags |= STATUS_FLAG_CLOSED;
            SEGGER_RTT_printf(0, "Valve: CLOSED\n");
            break;

        case VALVE_STATE_IDLE:
            drv8876_stop();
            drv8876_sleep();  /* Enter low power mode */
            m_status_flags &= ~STATUS_FLAG_MOVING;
            break;

        case VALVE_STATE_FAULT:
            drv8876_stop();
            drv8876_sleep();  /* Enter low power mode */
            m_status_flags &= ~STATUS_FLAG_MOVING;
            m_status_flags |= STATUS_FLAG_FAULT;
            SEGGER_RTT_printf(0, "Valve: FAULT\n");
            break;
    }
}

static void process_command(valve_cmd_t cmd)
{
    switch (cmd) {
        case VALVE_CMD_OPEN:
            if (!valve_is_open()) {
                enter_state(VALVE_STATE_OPENING);
            } else {
                SEGGER_RTT_printf(0, "Already open\n");
            }
            break;

        case VALVE_CMD_CLOSE:
            if (!valve_is_closed()) {
                enter_state(VALVE_STATE_CLOSING);
            } else {
                SEGGER_RTT_printf(0, "Already closed\n");
            }
            break;

        case VALVE_CMD_STOP:
            if (valve_is_open()) {
                enter_state(VALVE_STATE_OPEN);
            } else if (valve_is_closed()) {
                enter_state(VALVE_STATE_CLOSED);
            } else {
                enter_state(VALVE_STATE_IDLE);
            }
            break;

        case VALVE_CMD_EMERGENCY_CLOSE:
            enter_state(VALVE_STATE_CLOSING);
            break;

        default:
            break;
    }
}

static void update_state_machine(void)
{
    TickType_t now = xTaskGetTickCount();
    TickType_t elapsed = now - m_operation_start;

    switch (m_state) {
        case VALVE_STATE_OPENING:
            if (valve_is_open()) {
                enter_state(VALVE_STATE_OPEN);
            } else if (elapsed > pdMS_TO_TICKS(VALVE_TIMEOUT_MS)) {
                m_status_flags |= STATUS_FLAG_TIMEOUT;
                enter_state(VALVE_STATE_FAULT);
                SEGGER_RTT_printf(0, "Timeout opening\n");
            } else if (m_current_ma > VALVE_OVERCURRENT_MA) {
                m_status_flags |= STATUS_FLAG_OVERCURRENT;
                enter_state(VALVE_STATE_FAULT);
                SEGGER_RTT_printf(0, "Overcurrent: %d mA\n", m_current_ma);
            } else if (drv8876_check_fault()) {
                m_status_flags |= STATUS_FLAG_FAULT;
                enter_state(VALVE_STATE_FAULT);
                SEGGER_RTT_printf(0, "DRV8876 fault detected\n");
            }
            break;

        case VALVE_STATE_CLOSING:
            if (valve_is_closed()) {
                enter_state(VALVE_STATE_CLOSED);
            } else if (elapsed > pdMS_TO_TICKS(VALVE_TIMEOUT_MS)) {
                m_status_flags |= STATUS_FLAG_TIMEOUT;
                enter_state(VALVE_STATE_FAULT);
                SEGGER_RTT_printf(0, "Timeout closing\n");
            } else if (m_current_ma > VALVE_OVERCURRENT_MA) {
                m_status_flags |= STATUS_FLAG_OVERCURRENT;
                enter_state(VALVE_STATE_FAULT);
                SEGGER_RTT_printf(0, "Overcurrent: %d mA\n", m_current_ma);
            } else if (drv8876_check_fault()) {
                m_status_flags |= STATUS_FLAG_FAULT;
                enter_state(VALVE_STATE_FAULT);
                SEGGER_RTT_printf(0, "DRV8876 fault detected\n");
            }
            break;

        default:
            break;
    }
}

/* ==========================================================================
 * VALVE TASK
 * ========================================================================== */

void valve_task(void *pvParameters)
{
    (void)pvParameters;

    SEGGER_RTT_printf(0, "Valve task started\n");

    /* Configure limit switch pins as inputs with pullup */
    nrf_gpio_cfg_input(LIMIT_OPEN_PIN, NRF_GPIO_PIN_PULLUP);
    nrf_gpio_cfg_input(LIMIT_CLOSED_PIN, NRF_GPIO_PIN_PULLUP);

    /* Initialize hardware */
    drv8876_init();
    adc_init();

    /* Determine initial state */
    if (valve_is_open()) {
        m_state = VALVE_STATE_OPEN;
        m_status_flags |= STATUS_FLAG_OPEN;
        SEGGER_RTT_printf(0, "Initial state: OPEN\n");
    } else if (valve_is_closed()) {
        m_state = VALVE_STATE_CLOSED;
        m_status_flags |= STATUS_FLAG_CLOSED;
        SEGGER_RTT_printf(0, "Initial state: CLOSED\n");
    } else {
        m_state = VALVE_STATE_IDLE;
        SEGGER_RTT_printf(0, "Initial state: UNKNOWN\n");
    }

    TickType_t last_current_sample = 0;

    for (;;) {
        /* Check for pending commands */
        valve_cmd_t cmd = m_pending_cmd;
        if (cmd != VALVE_CMD_NONE) {
            m_pending_cmd = VALVE_CMD_NONE;
            process_command(cmd);
        }

        /* Sample current during motor operation */
        if (m_state == VALVE_STATE_OPENING || m_state == VALVE_STATE_CLOSING) {
            TickType_t now = xTaskGetTickCount();
            if (now - last_current_sample >= pdMS_TO_TICKS(50)) {  /* Sample every 50ms */
                m_current_ma = read_current_ma();
                last_current_sample = now;
            }
        }

        /* Update state machine */
        update_state_machine();

        /* Sleep */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* ==========================================================================
 * PUBLIC FUNCTIONS (thread-safe)
 * ========================================================================== */

void valve_request_open(void)
{
    m_pending_cmd = VALVE_CMD_OPEN;
}

void valve_request_close(void)
{
    m_pending_cmd = VALVE_CMD_CLOSE;
}

void valve_request_stop(void)
{
    m_pending_cmd = VALVE_CMD_STOP;
}

void valve_request_emergency_close(void)
{
    m_pending_cmd = VALVE_CMD_EMERGENCY_CLOSE;
}

valve_state_t valve_get_state(void)
{
    return m_state;
}

uint8_t valve_get_status_flags(void)
{
    return m_status_flags;
}

uint16_t valve_get_current_ma(void)
{
    return m_current_ma;
}
