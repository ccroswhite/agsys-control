/**
 * @file signal_processing.h
 * @brief Signal processing for hardware-synced Mag Meter
 * 
 * Processes ADC samples that are hardware-triggered at optimal times
 * within each coil half-cycle.
 */

#ifndef SIGNAL_PROCESSING_H
#define SIGNAL_PROCESSING_H

#include <Arduino.h>

// Initialize signal processing
void signal_init(void);

// Add sample to processing buffer (called from ADC trigger callback)
void signal_addSample(int32_t electrode, int32_t current, bool polarity);

// Compute and return flow signal (call once per averaging window)
float signal_computeFlowSignal(void);

// Get last computed flow signal (doesn't recompute)
float signal_getFlowSignal(void);

// Compute and return coil current
float signal_computeCoilCurrent(void);

// Get last computed coil current
float signal_getCoilCurrent(void);

// Get total sample count since last reset
uint32_t signal_getSampleCount(void);

// Reset signal buffers (call after computing flow)
void signal_reset(void);

#endif // SIGNAL_PROCESSING_H
