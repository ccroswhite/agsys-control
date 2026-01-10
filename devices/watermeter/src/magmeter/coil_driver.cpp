/**
 * @file coil_driver.cpp
 * @brief Coil excitation driver with software-timed ADC triggering
 * 
 * Uses Arduino timer functions for coil excitation timing.
 * For production, this could be upgraded to use hardware timers via
 * direct register access if the nrfx driver is not available.
 * 
 * Timing diagram:
 *   Coil:  ____/‾‾‾‾‾‾‾‾\________/‾‾‾‾‾‾‾‾\____
 *   ADC:       S    S    S    S    S    S    S
 *              ^settling  ^sample   ^settling
 */

#include "coil_driver.h"
#include "magmeter_config.h"
#include <Arduino.h>

// State
static uint16_t currentFrequency = 1000;
static volatile bool currentPolarity = false;
static volatile uint32_t polarityChangeCount = 0;
static bool isRunning = false;

// Timing
static uint32_t halfPeriodUs = 500;
static uint32_t lastToggleUs = 0;
static uint32_t lastAdcTriggerUs = 0;
static uint32_t adcSampleIntervalUs = 45;
static uint8_t adcSamplesThisCycle = 0;

// Callbacks
static coil_polarity_callback_t polarityCallback = NULL;
static coil_adc_trigger_callback_t adcTriggerCallback = NULL;

void coil_init(uint16_t frequency_hz) {
    currentFrequency = frequency_hz;
    
    // Configure coil gate pin as output
    pinMode(PIN_COIL_GATE, OUTPUT);
    digitalWrite(PIN_COIL_GATE, LOW);
    
    // Calculate timer period for half-cycle (coil toggles at 2x frequency)
    halfPeriodUs = 500000UL / frequency_hz;
    
    // ADC sample interval: we want ~10 samples per half-cycle after settling
    uint32_t settling_us = COIL_SETTLING_TIME_US;
    uint32_t sample_window_us = halfPeriodUs - settling_us;
    adcSampleIntervalUs = sample_window_us / SAMPLES_PER_HALF_CYCLE;
    
    if (adcSampleIntervalUs < 20) adcSampleIntervalUs = 20;  // Minimum 20µs
    
    DEBUG_PRINTF("Coil driver initialized: %d Hz, half-period=%lu us\n", 
                 frequency_hz, halfPeriodUs);
    DEBUG_PRINTF("ADC sample interval: %lu us (%d samples/half-cycle)\n",
                 adcSampleIntervalUs, SAMPLES_PER_HALF_CYCLE);
}

void coil_start(void) {
    if (isRunning) return;
    
    isRunning = true;
    currentPolarity = false;
    polarityChangeCount = 0;
    adcSamplesThisCycle = 0;
    
    // Start with coil off
    digitalWrite(PIN_COIL_GATE, LOW);
    
    lastToggleUs = micros();
    lastAdcTriggerUs = lastToggleUs + COIL_SETTLING_TIME_US;
    
    DEBUG_PRINTLN("Coil excitation started");
}

void coil_stop(void) {
    if (!isRunning) return;
    
    isRunning = false;
    
    // Ensure coil is off
    digitalWrite(PIN_COIL_GATE, LOW);
    
    DEBUG_PRINTLN("Coil excitation stopped");
}

// Call this from main loop - handles timing
void coil_update(void) {
    if (!isRunning) return;
    
    uint32_t now = micros();
    
    // Check if it's time to toggle polarity
    if ((now - lastToggleUs) >= halfPeriodUs) {
        currentPolarity = !currentPolarity;
        digitalWrite(PIN_COIL_GATE, currentPolarity ? HIGH : LOW);
        polarityChangeCount++;
        lastToggleUs = now;
        adcSamplesThisCycle = 0;
        
        // Notify callback of polarity change
        if (polarityCallback) {
            polarityCallback(currentPolarity);
        }
        
        // Reset ADC trigger timing (wait for settling)
        lastAdcTriggerUs = now + COIL_SETTLING_TIME_US;
    }
    
    // Check if it's time for an ADC sample (after settling, within sample window)
    uint32_t timeSinceToggle = now - lastToggleUs;
    if (timeSinceToggle >= COIL_SETTLING_TIME_US && 
        adcSamplesThisCycle < SAMPLES_PER_HALF_CYCLE &&
        (now - lastAdcTriggerUs) >= adcSampleIntervalUs) {
        
        lastAdcTriggerUs = now;
        adcSamplesThisCycle++;
        
        // Trigger ADC sample
        if (adcTriggerCallback) {
            adcTriggerCallback(currentPolarity);
        }
    }
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
