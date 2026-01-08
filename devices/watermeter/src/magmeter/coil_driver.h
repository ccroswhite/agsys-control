/**
 * @file coil_driver.h
 * @brief Coil excitation driver with hardware-synced ADC triggering
 * 
 * Uses nRF52840 hardware timers to generate precise square wave excitation
 * and trigger ADC sampling at optimal points within each half-cycle.
 */

#ifndef COIL_DRIVER_H
#define COIL_DRIVER_H

#include <Arduino.h>

// Callback types
typedef void (*coil_polarity_callback_t)(bool polarity);
typedef void (*coil_adc_trigger_callback_t)(bool polarity);

// Initialize coil driver with specified frequency
void coil_init(uint16_t frequency_hz);

// Start coil excitation
void coil_start(void);

// Stop coil excitation
void coil_stop(void);

// Get current polarity state
bool coil_getPolarity(void);

// Get polarity change count (for diagnostics)
uint32_t coil_getPolarityCount(void);

// Set excitation frequency
void coil_setFrequency(uint16_t frequency_hz);

// Get current frequency
uint16_t coil_getFrequency(void);

// Set callback for polarity changes
void coil_setPolarityCallback(coil_polarity_callback_t callback);

// Set callback for ADC trigger (called at optimal sample times)
void coil_setAdcTriggerCallback(coil_adc_trigger_callback_t callback);

// Check if coil is running
bool coil_isRunning(void);

#endif // COIL_DRIVER_H
