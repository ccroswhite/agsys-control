/**
 * @file signal_processing.cpp
 * @brief Signal processing for hardware-synced Mag Meter
 * 
 * Implements synchronous detection using samples that are hardware-triggered
 * at optimal times within each coil half-cycle. The coil driver handles
 * settling time, so all samples received here are valid.
 * 
 * Flow signal extraction:
 *   V_flow = (V_positive - V_negative) / 2
 * 
 * This rejects:
 *   - Common-mode noise (appears equally in both half-cycles)
 *   - DC offset (cancels out in subtraction)
 *   - 1/f noise (averaged over many samples)
 */

#include "signal_processing.h"
#include "magmeter_config.h"
#include "calibration.h"

// Accumulator buffers for synchronous detection
static int64_t sumPositive = 0;
static int64_t sumNegative = 0;
static int64_t sumCurrent = 0;
static uint32_t countPositive = 0;
static uint32_t countNegative = 0;
static uint32_t countCurrent = 0;

// Last computed values
static float lastFlowSignal = 0.0f;
static float lastCoilCurrent = 0.0f;

// Sample count for averaging window
static uint32_t totalSamples = 0;

void signal_init(void) {
    signal_reset();
}

void signal_addSample(int32_t electrode, int32_t current, bool polarity) {
    // Apply zero offset calibration to electrode reading
    int32_t calibratedElectrode = calibration_applyZero(electrode);
    
    // Accumulate based on polarity
    // Samples are already taken at optimal times (after settling)
    if (polarity) {
        sumPositive += calibratedElectrode;
        countPositive++;
    } else {
        sumNegative += calibratedElectrode;
        countNegative++;
    }
    
    // Always accumulate current for monitoring
    sumCurrent += current;
    countCurrent++;
    totalSamples++;
}

float signal_computeFlowSignal(void) {
    if (countPositive == 0 || countNegative == 0) {
        lastFlowSignal = 0.0f;
        return 0.0f;
    }
    
    // Calculate average for each half-cycle
    float avgPositive = (float)sumPositive / countPositive;
    float avgNegative = (float)sumNegative / countNegative;
    
    // Flow signal is the difference (synchronous detection)
    // Divide by 2 to get amplitude (not peak-to-peak)
    float flowSignal = (avgPositive - avgNegative) / 2.0f;
    
    // Apply span calibration
    flowSignal = calibration_applySpan(flowSignal);
    
    lastFlowSignal = flowSignal;
    return flowSignal;
}

float signal_getFlowSignal(void) {
    return lastFlowSignal;
}

float signal_computeCoilCurrent(void) {
    if (countCurrent == 0) {
        lastCoilCurrent = 0.0f;
        return 0.0f;
    }
    
    lastCoilCurrent = (float)sumCurrent / countCurrent;
    return lastCoilCurrent;
}

float signal_getCoilCurrent(void) {
    return lastCoilCurrent;
}

uint32_t signal_getSampleCount(void) {
    return totalSamples;
}

void signal_reset(void) {
    sumPositive = 0;
    sumNegative = 0;
    sumCurrent = 0;
    countPositive = 0;
    countNegative = 0;
    countCurrent = 0;
    totalSamples = 0;
    // Don't reset lastFlowSignal/lastCoilCurrent - keep for display
}
