/**
 * @file valve_task.c
 * @brief Solenoid valve control task implementation
 * 
 * Controls 24V AC solenoid valves via optoisolated TRIAC.
 * Supports both NO (Normally Open) and NC (Normally Closed) valve types.
 */

#include "sdk_config.h"
#include "FreeRTOS.h"
#include "task.h"

#include "nrf_gpio.h"
#include "nrf_delay.h"
#include "SEGGER_RTT.h"

#include "valve_task.h"
#include "board_config.h"

/* ==========================================================================
 * PRIVATE DATA
 * ========================================================================== */

static valve_state_t m_state = VALVE_STATE_IDLE;
static uint8_t m_status_flags = 0;
static bool m_solenoid_energized = false;
static bool m_valve_type_nc = false;  /* false = NO, true = NC */

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
 * TRIAC SOLENOID CONTROL
 * ========================================================================== */

static void solenoid_init(void)
{
    /* Configure solenoid control pin as output */
    nrf_gpio_cfg_output(SOLENOID_CTRL_PIN);
    
    /* Configure zero-cross detection as input */
    nrf_gpio_cfg_input(ZERO_CROSS_PIN, NRF_GPIO_PIN_NOPULL);
    
    /* Configure NO/NC DIP switch as input with pullup */
    nrf_gpio_cfg_input(DIP_NONC_PIN, NRF_GPIO_PIN_PULLUP);
    
    /* Start with solenoid de-energized */
    nrf_gpio_pin_clear(SOLENOID_CTRL_PIN);
    m_solenoid_energized = false;
    
    /* Read valve type configuration (active low: ON = NC) */
    nrf_delay_us(10);
    m_valve_type_nc = (nrf_gpio_pin_read(DIP_NONC_PIN) == 0);
    
    SEGGER_RTT_printf(0, "Solenoid initialized, type: %s\n", 
                      m_valve_type_nc ? "NC" : "NO");
}

static void solenoid_energize(void)
{
    if (!m_solenoid_energized) {
        nrf_gpio_pin_set(SOLENOID_CTRL_PIN);
        m_solenoid_energized = true;
        SEGGER_RTT_printf(0, "Solenoid: ENERGIZED\n");
    }
}

static void solenoid_deenergize(void)
{
    if (m_solenoid_energized) {
        nrf_gpio_pin_clear(SOLENOID_CTRL_PIN);
        m_solenoid_energized = false;
        SEGGER_RTT_printf(0, "Solenoid: DE-ENERGIZED\n");
    }
}

/* ==========================================================================
 * VALVE STATE HELPERS
 * For solenoid valves, position is determined by energized state and valve type
 * ========================================================================== */

bool valve_is_open(void)
{
    /* NO valve: open when de-energized
     * NC valve: open when energized */
    if (m_valve_type_nc) {
        return m_solenoid_energized;
    } else {
        return !m_solenoid_energized;
    }
}

bool valve_is_closed(void)
{
    return !valve_is_open();
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
            /* For solenoid valves, "opening" is instantaneous */
            /* NO valve: de-energize to open */
            /* NC valve: energize to open */
            m_status_flags &= ~(STATUS_FLAG_CLOSED | STATUS_FLAG_FAULT);
            if (m_valve_type_nc) {
                solenoid_energize();
            } else {
                solenoid_deenergize();
            }
            /* Immediately transition to OPEN state */
            m_state = VALVE_STATE_OPEN;
            m_status_flags |= STATUS_FLAG_OPEN;
            SEGGER_RTT_printf(0, "Valve: OPEN\n");
            break;

        case VALVE_STATE_CLOSING:
            /* For solenoid valves, "closing" is instantaneous */
            /* NO valve: energize to close */
            /* NC valve: de-energize to close */
            m_status_flags &= ~(STATUS_FLAG_OPEN | STATUS_FLAG_FAULT);
            if (m_valve_type_nc) {
                solenoid_deenergize();
            } else {
                solenoid_energize();
            }
            /* Immediately transition to CLOSED state */
            m_state = VALVE_STATE_CLOSED;
            m_status_flags |= STATUS_FLAG_CLOSED;
            SEGGER_RTT_printf(0, "Valve: CLOSED\n");
            break;

        case VALVE_STATE_OPEN:
            m_status_flags &= ~STATUS_FLAG_CLOSED;
            m_status_flags |= STATUS_FLAG_OPEN;
            SEGGER_RTT_printf(0, "Valve: OPEN\n");
            break;

        case VALVE_STATE_CLOSED:
            m_status_flags &= ~STATUS_FLAG_OPEN;
            m_status_flags |= STATUS_FLAG_CLOSED;
            SEGGER_RTT_printf(0, "Valve: CLOSED\n");
            break;

        case VALVE_STATE_IDLE:
            /* De-energize solenoid for safety */
            solenoid_deenergize();
            m_status_flags &= ~(STATUS_FLAG_OPEN | STATUS_FLAG_CLOSED);
            break;

        case VALVE_STATE_FAULT:
            /* De-energize solenoid on fault */
            solenoid_deenergize();
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
    /* For solenoid valves, state transitions are instantaneous.
     * No timeout or current monitoring needed - the TRIAC either
     * conducts or it doesn't. The MOC3021 has built-in zero-cross
     * detection for clean switching.
     * 
     * Future enhancement: could monitor zero-cross signal to detect
     * AC power loss condition.
     */
}

/* ==========================================================================
 * VALVE TASK
 * ========================================================================== */

void valve_task(void *pvParameters)
{
    (void)pvParameters;

    SEGGER_RTT_printf(0, "Solenoid valve task started\n");

    /* Initialize solenoid control */
    solenoid_init();

    /* Determine initial state based on solenoid energized state and valve type */
    if (valve_is_open()) {
        m_state = VALVE_STATE_OPEN;
        m_status_flags |= STATUS_FLAG_OPEN;
        SEGGER_RTT_printf(0, "Initial state: OPEN\n");
    } else {
        m_state = VALVE_STATE_CLOSED;
        m_status_flags |= STATUS_FLAG_CLOSED;
        SEGGER_RTT_printf(0, "Initial state: CLOSED\n");
    }

    for (;;) {
        /* Check for pending commands */
        valve_cmd_t cmd = m_pending_cmd;
        if (cmd != VALVE_CMD_NONE) {
            m_pending_cmd = VALVE_CMD_NONE;
            process_command(cmd);
        }

        /* Update state machine (minimal for solenoid valves) */
        update_state_machine();

        /* Sleep - solenoid control doesn't need fast polling */
        vTaskDelay(pdMS_TO_TICKS(50));
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
    /* Solenoid valves don't have current sensing */
    return 0;
}
