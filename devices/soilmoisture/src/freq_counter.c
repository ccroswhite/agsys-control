/**
 * @file freq_counter.c
 * @brief Frequency counter implementation using TIMER + GPIOTE + PPI
 * 
 * Technique: Use TIMER2 in counter mode, triggered by GPIOTE events.
 * PPI connects GPIOTE IN event to TIMER COUNT task.
 * A separate TIMER1 provides the measurement window.
 * 
 * Uses direct register access for PPI to avoid SoftDevice conflicts.
 * PPI channel 0 is used (channels 0-7 are available for app use with S132).
 */

#include "sdk_config.h"
#include "freq_counter.h"
#include "board_config.h"

#include "nrf.h"
#include "nrf_gpio.h"
#include "nrf_gpiote.h"
#include "nrf_timer.h"
#include "nrfx_gpiote.h"
#include "nrfx_timer.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "SEGGER_RTT.h"

/* ==========================================================================
 * HARDWARE CONFIGURATION
 * ========================================================================== */

/* TIMER2 for pulse counting (counter mode) */
static const nrfx_timer_t m_counter_timer = NRFX_TIMER_INSTANCE(2);

/* TIMER1 for measurement window */
static const nrfx_timer_t m_window_timer = NRFX_TIMER_INSTANCE(1);

/* PPI channel (use channel 0, available for app with S132) */
#define PPI_CHANNEL     0

/* GPIOTE channel for frequency input */
#define GPIOTE_CHANNEL  0

/* Probe pin mapping */
static const uint8_t m_probe_pins[MAX_PROBES] = {
    PROBE_1_FREQ_PIN,
#if MAX_PROBES > 1
    PROBE_2_FREQ_PIN,
#endif
#if MAX_PROBES > 2
    PROBE_3_FREQ_PIN,
#endif
#if MAX_PROBES > 3
    PROBE_4_FREQ_PIN,
#endif
};

/* Measurement complete flag */
static volatile bool m_measurement_done = false;

/* Initialization flag */
static bool m_initialized = false;

/* ==========================================================================
 * TIMER HANDLERS
 * ========================================================================== */

static void window_timer_handler(nrf_timer_event_t event_type, void *p_context)
{
    (void)p_context;
    
    if (event_type == NRF_TIMER_EVENT_COMPARE0) {
        m_measurement_done = true;
    }
}

static void counter_timer_handler(nrf_timer_event_t event_type, void *p_context)
{
    (void)event_type;
    (void)p_context;
    /* Counter timer doesn't generate interrupts in counter mode */
}

/* ==========================================================================
 * INITIALIZATION
 * ========================================================================== */

bool freq_counter_init(void)
{
    if (m_initialized) {
        return true;
    }
    
    ret_code_t err;
    
    /* Configure probe power pin */
    nrf_gpio_cfg_output(PROBE_POWER_PIN);
    nrf_gpio_pin_set(PROBE_POWER_PIN);  /* Power off (active LOW) */
    
    /* Configure probe input pins */
    for (int i = 0; i < MAX_PROBES; i++) {
        nrf_gpio_cfg_input(m_probe_pins[i], NRF_GPIO_PIN_NOPULL);
    }
    
    /* Initialize GPIOTE if not already done */
    if (!nrfx_gpiote_is_init()) {
        err = nrfx_gpiote_init();
        if (err != NRFX_SUCCESS) {
            SEGGER_RTT_printf(0, "FreqCnt: GPIOTE init failed: %d\n", err);
            return false;
        }
    }
    
    /* Initialize counter timer (TIMER2) in counter mode */
    nrfx_timer_config_t counter_config = NRFX_TIMER_DEFAULT_CONFIG;
    counter_config.mode = NRF_TIMER_MODE_COUNTER;
    counter_config.bit_width = NRF_TIMER_BIT_WIDTH_32;
    
    err = nrfx_timer_init(&m_counter_timer, &counter_config, counter_timer_handler);
    if (err != NRFX_SUCCESS) {
        SEGGER_RTT_printf(0, "FreqCnt: Counter timer init failed: %d\n", err);
        return false;
    }
    
    /* Initialize window timer (TIMER1) for measurement timing */
    nrfx_timer_config_t window_config = NRFX_TIMER_DEFAULT_CONFIG;
    window_config.frequency = NRF_TIMER_FREQ_1MHz;
    window_config.mode = NRF_TIMER_MODE_TIMER;
    window_config.bit_width = NRF_TIMER_BIT_WIDTH_32;
    
    err = nrfx_timer_init(&m_window_timer, &window_config, window_timer_handler);
    if (err != NRFX_SUCCESS) {
        SEGGER_RTT_printf(0, "FreqCnt: Window timer init failed: %d\n", err);
        return false;
    }
    
    m_initialized = true;
    SEGGER_RTT_printf(0, "FreqCnt: Initialized\n");
    return true;
}

/* ==========================================================================
 * POWER CONTROL
 * ========================================================================== */

void freq_counter_power_on(void)
{
    nrf_gpio_pin_clear(PROBE_POWER_PIN);  /* Active LOW */
    SEGGER_RTT_printf(0, "FreqCnt: Power ON\n");
}

void freq_counter_power_off(void)
{
    nrf_gpio_pin_set(PROBE_POWER_PIN);
    SEGGER_RTT_printf(0, "FreqCnt: Power OFF\n");
}

/* ==========================================================================
 * FREQUENCY MEASUREMENT
 * ========================================================================== */

uint32_t freq_counter_measure(uint8_t probe_index, uint32_t measurement_ms)
{
    if (!m_initialized || probe_index >= MAX_PROBES) {
        return 0;
    }
    
    uint8_t pin = m_probe_pins[probe_index];
    
    SEGGER_RTT_printf(0, "FreqCnt: Measuring probe %d (pin %d) for %lu ms\n",
                      probe_index, pin, measurement_ms);
    
    /* Configure GPIOTE channel for rising edge on probe pin */
    NRF_GPIOTE->CONFIG[GPIOTE_CHANNEL] = 
        (GPIOTE_CONFIG_MODE_Event << GPIOTE_CONFIG_MODE_Pos) |
        (pin << GPIOTE_CONFIG_PSEL_Pos) |
        (GPIOTE_CONFIG_POLARITY_LoToHi << GPIOTE_CONFIG_POLARITY_Pos);
    
    /* Get addresses for PPI */
    uint32_t gpiote_evt_addr = (uint32_t)&NRF_GPIOTE->EVENTS_IN[GPIOTE_CHANNEL];
    uint32_t timer_task_addr = nrfx_timer_task_address_get(&m_counter_timer, 
                                                           NRF_TIMER_TASK_COUNT);
    
    /* Configure PPI channel: GPIOTE event -> TIMER COUNT */
    NRF_PPI->CH[PPI_CHANNEL].EEP = gpiote_evt_addr;
    NRF_PPI->CH[PPI_CHANNEL].TEP = timer_task_addr;
    
    /* Clear and prepare counter timer */
    nrfx_timer_clear(&m_counter_timer);
    
    /* Configure window timer for measurement duration */
    uint32_t ticks = nrfx_timer_us_to_ticks(&m_window_timer, measurement_ms * 1000);
    nrfx_timer_extended_compare(&m_window_timer, NRF_TIMER_CC_CHANNEL0, ticks,
                                 NRF_TIMER_SHORT_COMPARE0_STOP_MASK, true);
    nrfx_timer_clear(&m_window_timer);
    
    /* Reset measurement flag */
    m_measurement_done = false;
    
    /* Clear GPIOTE event */
    NRF_GPIOTE->EVENTS_IN[GPIOTE_CHANNEL] = 0;
    
    /* Enable PPI channel */
    NRF_PPI->CHENSET = (1 << PPI_CHANNEL);
    
    /* Start both timers */
    nrfx_timer_enable(&m_counter_timer);
    nrfx_timer_enable(&m_window_timer);
    
    /* Wait for measurement to complete */
    while (!m_measurement_done) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    /* Read counter value */
    uint32_t count = nrfx_timer_capture(&m_counter_timer, NRF_TIMER_CC_CHANNEL1);
    
    /* Stop and cleanup */
    nrfx_timer_disable(&m_counter_timer);
    nrfx_timer_disable(&m_window_timer);
    
    /* Disable PPI channel */
    NRF_PPI->CHENCLR = (1 << PPI_CHANNEL);
    
    /* Disable GPIOTE channel */
    NRF_GPIOTE->CONFIG[GPIOTE_CHANNEL] = 0;
    
    /* Calculate frequency: count / (measurement_ms / 1000) = count * 1000 / measurement_ms */
    uint32_t freq_hz = (count * 1000) / measurement_ms;
    
    SEGGER_RTT_printf(0, "FreqCnt: Probe %d: count=%lu, freq=%lu Hz\n",
                      probe_index, count, freq_hz);
    
    return freq_hz;
}

bool freq_counter_is_valid(uint32_t freq_hz)
{
    return (freq_hz >= FREQ_MIN_VALID_HZ && freq_hz <= FREQ_MAX_VALID_HZ);
}
