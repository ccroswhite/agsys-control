/**
 * @file coil_driver.cpp
 * @brief Coil excitation driver with hardware-synced ADC triggering
 * 
 * Uses nRF52840 TIMER peripheral to generate precise square wave excitation
 * and trigger ADC sampling at optimal points within each half-cycle.
 * 
 * Timing diagram:
 *   Coil:  ____/‾‾‾‾‾‾‾‾\________/‾‾‾‾‾‾‾‾\____
 *   ADC:       S    S    S    S    S    S    S
 *              ^settling  ^sample   ^settling
 * 
 * ADC samples are triggered after settling time, multiple times per half-cycle.
 */

#include "coil_driver.h"
#include "magmeter_config.h"
#include <nrfx_timer.h>
#include <nrfx_gpiote.h>
#include <nrfx_ppi.h>

// Use TIMER2 for coil excitation (TIMER0/1 used by SoftDevice/Arduino)
static const nrfx_timer_t coil_timer = NRFX_TIMER_INSTANCE(2);

// Use TIMER3 for ADC sync pulses
static const nrfx_timer_t adc_timer = NRFX_TIMER_INSTANCE(3);

// State
static uint16_t currentFrequency = 1000;
static volatile bool currentPolarity = false;
static volatile uint32_t polarityChangeCount = 0;
static bool isRunning = false;

// Callbacks
static coil_polarity_callback_t polarityCallback = NULL;
static coil_adc_trigger_callback_t adcTriggerCallback = NULL;

// PPI channels for hardware triggering
static nrf_ppi_channel_t ppi_coil_toggle;
static nrf_ppi_channel_t ppi_adc_trigger;

// Timer event handler for coil polarity changes
static void coil_timer_handler(nrf_timer_event_t event_type, void* p_context) {
    (void)p_context;
    
    if (event_type == NRF_TIMER_EVENT_COMPARE0) {
        // Toggle coil output
        currentPolarity = !currentPolarity;
        nrf_gpio_pin_write(PIN_COIL_GATE, currentPolarity ? 1 : 0);
        polarityChangeCount++;
        
        // Notify callback of polarity change
        if (polarityCallback) {
            polarityCallback(currentPolarity);
        }
    }
}

// Timer event handler for ADC trigger
static void adc_timer_handler(nrf_timer_event_t event_type, void* p_context) {
    (void)p_context;
    
    if (event_type == NRF_TIMER_EVENT_COMPARE0) {
        // Trigger ADC sample
        if (adcTriggerCallback) {
            adcTriggerCallback(currentPolarity);
        }
    }
}

void coil_init(uint16_t frequency_hz) {
    currentFrequency = frequency_hz;
    
    // Configure coil gate pin as output
    nrf_gpio_cfg_output(PIN_COIL_GATE);
    nrf_gpio_pin_clear(PIN_COIL_GATE);
    
    // Calculate timer period for half-cycle (coil toggles at 2x frequency)
    // Timer runs at 1 MHz (16 MHz / 16 prescaler)
    uint32_t half_period_us = 500000UL / frequency_hz;
    
    // Configure coil timer
    nrfx_timer_config_t timer_cfg = {
        .frequency = NRF_TIMER_FREQ_1MHz,
        .mode = NRF_TIMER_MODE_TIMER,
        .bit_width = NRF_TIMER_BIT_WIDTH_32,
        .interrupt_priority = NRFX_TIMER_DEFAULT_CONFIG_IRQ_PRIORITY,
        .p_context = NULL
    };
    
    nrfx_timer_init(&coil_timer, &timer_cfg, coil_timer_handler);
    nrfx_timer_extended_compare(&coil_timer, NRF_TIMER_CC_CHANNEL0, 
                                 half_period_us, 
                                 NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK, 
                                 true);  // Enable interrupt
    
    // Configure ADC trigger timer
    // Triggers multiple samples per half-cycle, after settling time
    nrfx_timer_init(&adc_timer, &timer_cfg, adc_timer_handler);
    
    // ADC sample interval: we want ~10 samples per half-cycle after settling
    // At 1kHz coil = 500µs half-cycle, settling = 50µs, leaves 450µs
    // Sample every 45µs = 10 samples per half-cycle
    uint32_t settling_us = COIL_SETTLING_TIME_US;
    uint32_t sample_window_us = half_period_us - settling_us;
    uint32_t sample_interval_us = sample_window_us / SAMPLES_PER_HALF_CYCLE;
    
    if (sample_interval_us < 20) sample_interval_us = 20;  // Minimum 20µs
    
    nrfx_timer_extended_compare(&adc_timer, NRF_TIMER_CC_CHANNEL0,
                                 sample_interval_us,
                                 NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK,
                                 true);
    
    DEBUG_PRINTF("Coil driver initialized: %d Hz, half-period=%lu us\n", 
                 frequency_hz, half_period_us);
    DEBUG_PRINTF("ADC sample interval: %lu us (%d samples/half-cycle)\n",
                 sample_interval_us, SAMPLES_PER_HALF_CYCLE);
}

void coil_start(void) {
    if (isRunning) return;
    
    isRunning = true;
    currentPolarity = false;
    polarityChangeCount = 0;
    
    // Start with coil off
    nrf_gpio_pin_clear(PIN_COIL_GATE);
    
    // Clear and start timers
    nrfx_timer_clear(&coil_timer);
    nrfx_timer_clear(&adc_timer);
    nrfx_timer_enable(&coil_timer);
    
    // Delay ADC timer start by settling time
    // This ensures first ADC sample is after coil has settled
    delayMicroseconds(COIL_SETTLING_TIME_US);
    nrfx_timer_enable(&adc_timer);
    
    DEBUG_PRINTLN("Coil excitation started (hardware sync)");
}

void coil_stop(void) {
    if (!isRunning) return;
    
    isRunning = false;
    
    // Stop timers
    nrfx_timer_disable(&coil_timer);
    nrfx_timer_disable(&adc_timer);
    
    // Ensure coil is off
    nrf_gpio_pin_clear(PIN_COIL_GATE);
    
    DEBUG_PRINTLN("Coil excitation stopped");
}

bool coil_getPolarity(void) {
    return currentPolarity;
}

uint32_t coil_getPolarityCount(void) {
    return polarityChangeCount;
}

void coil_setFrequency(uint16_t frequency_hz) {
    bool wasRunning = isRunning;
    
    if (wasRunning) {
        coil_stop();
    }
    
    // Reinitialize with new frequency
    nrfx_timer_uninit(&coil_timer);
    nrfx_timer_uninit(&adc_timer);
    coil_init(frequency_hz);
    
    if (wasRunning) {
        coil_start();
    }
    
    DEBUG_PRINTF("Coil frequency changed to %d Hz\n", frequency_hz);
}

uint16_t coil_getFrequency(void) {
    return currentFrequency;
}

void coil_setPolarityCallback(coil_polarity_callback_t callback) {
    polarityCallback = callback;
}

void coil_setAdcTriggerCallback(coil_adc_trigger_callback_t callback) {
    adcTriggerCallback = callback;
}

bool coil_isRunning(void) {
    return isRunning;
}
