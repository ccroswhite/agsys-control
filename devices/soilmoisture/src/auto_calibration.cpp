/**
 * @file auto_calibration.cpp
 * @brief Auto-Calibration Module Implementation
 * 
 * Implements adaptive f_air calibration with stability detection.
 */

#include <Arduino.h>
#include <math.h>
#include "auto_calibration.h"
#include "moisture_probe.h"
#include "moisture_cal.h"
#include "config.h"

// LED control (simple implementation - can be replaced with LED driver)
static void ledOn() { digitalWrite(PIN_LED_STATUS, HIGH); }
static void ledOff() { digitalWrite(PIN_LED_STATUS, LOW); }
static void ledToggle() { digitalWrite(PIN_LED_STATUS, !digitalRead(PIN_LED_STATUS)); }

// Rolling window for stability detection
static uint32_t s_samples[CAL_WINDOW_SIZE];
static uint8_t s_sampleIndex = 0;
static bool s_windowFull = false;

// Calibration state
static volatile AutoCalState s_state = AUTO_CAL_STATE_IDLE;
static volatile bool s_abortRequested = false;
static AutoCalProgress s_progress;

// Forward declarations
static float computeMean(void);
static float computeStdDev(float mean);
static uint32_t computeTrimmedMean(void);
static void resetWindow(void);

/**
 * @brief Initialize auto-calibration module
 */
void autoCal_init(void) {
    pinMode(PIN_LED_STATUS, OUTPUT);
    ledOff();
    resetWindow();
    s_state = AUTO_CAL_STATE_IDLE;
    s_abortRequested = false;
}

/**
 * @brief Reset the sample window
 */
static void resetWindow(void) {
    memset(s_samples, 0, sizeof(s_samples));
    s_sampleIndex = 0;
    s_windowFull = false;
}

/**
 * @brief Add a sample to the rolling window
 */
static void addSample(uint32_t freq) {
    s_samples[s_sampleIndex] = freq;
    s_sampleIndex = (s_sampleIndex + 1) % CAL_WINDOW_SIZE;
    if (s_sampleIndex == 0) {
        s_windowFull = true;
    }
}

/**
 * @brief Compute mean of samples in window
 */
static float computeMean(void) {
    uint32_t count = s_windowFull ? CAL_WINDOW_SIZE : s_sampleIndex;
    if (count == 0) return 0;
    
    uint64_t sum = 0;
    for (uint32_t i = 0; i < count; i++) {
        sum += s_samples[i];
    }
    return (float)sum / count;
}

/**
 * @brief Compute standard deviation
 */
static float computeStdDev(float mean) {
    uint32_t count = s_windowFull ? CAL_WINDOW_SIZE : s_sampleIndex;
    if (count < 2) return 0;
    
    float sumSqDiff = 0;
    for (uint32_t i = 0; i < count; i++) {
        float diff = (float)s_samples[i] - mean;
        sumSqDiff += diff * diff;
    }
    return sqrtf(sumSqDiff / (count - 1));
}

/**
 * @brief Compute trimmed mean (discard top/bottom 10%)
 */
static uint32_t computeTrimmedMean(void) {
    uint32_t count = s_windowFull ? CAL_WINDOW_SIZE : s_sampleIndex;
    if (count < 5) {
        // Not enough samples for trimming, use regular mean
        return (uint32_t)computeMean();
    }
    
    // Copy and sort samples
    uint32_t sorted[CAL_WINDOW_SIZE];
    memcpy(sorted, s_samples, count * sizeof(uint32_t));
    
    // Simple bubble sort (small array)
    for (uint32_t i = 0; i < count - 1; i++) {
        for (uint32_t j = 0; j < count - i - 1; j++) {
            if (sorted[j] > sorted[j + 1]) {
                uint32_t temp = sorted[j];
                sorted[j] = sorted[j + 1];
                sorted[j + 1] = temp;
            }
        }
    }
    
    // Trim 10% from each end
    uint32_t trimCount = count / 10;
    if (trimCount < 1) trimCount = 1;
    
    uint32_t start = trimCount;
    uint32_t end = count - trimCount;
    
    uint64_t sum = 0;
    for (uint32_t i = start; i < end; i++) {
        sum += sorted[i];
    }
    
    return (uint32_t)(sum / (end - start));
}

/**
 * @brief Check if any probe needs f_air calibration
 */
bool autoCal_needed(void) {
    for (uint8_t i = 0; i < NUM_MOISTURE_PROBES; i++) {
        if (moistureCal_needsAirCal(i)) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Run auto-calibration for a single probe
 */
uint32_t autoCal_runSingle(uint8_t probeIndex, AutoCalResult* result) {
    if (probeIndex >= NUM_MOISTURE_PROBES) {
        if (result) *result = AUTO_CAL_PROBE_ERROR;
        return 0;
    }
    
    DEBUG_PRINTF("AutoCal: Starting calibration for probe %d\n", probeIndex);
    
    // Initialize
    resetWindow();
    s_state = AUTO_CAL_STATE_WARMUP;
    s_abortRequested = false;
    s_progress.probeIndex = probeIndex;
    s_progress.elapsedMs = 0;
    s_progress.currentFreq = 0;
    s_progress.stability = 1.0f;
    s_progress.isStable = false;
    
    uint32_t startTime = millis();
    uint32_t lastSampleTime = 0;
    uint32_t lastLedToggle = 0;
    uint32_t bestFreq = 0;
    float bestStability = 1.0f;
    
    // Power on probes
    moistureProbe_powerOn();
    
    // LED: slow blink during warmup
    ledOn();
    
    while (!s_abortRequested) {
        uint32_t now = millis();
        uint32_t elapsed = now - startTime;
        s_progress.elapsedMs = elapsed;
        
        // Check timeout
        if (elapsed >= CAL_MAX_DURATION_MS) {
            DEBUG_PRINTLN("AutoCal: Timeout reached");
            s_state = AUTO_CAL_STATE_ERROR;
            break;
        }
        
        // Update state based on elapsed time
        if (elapsed < CAL_MIN_DURATION_MS) {
            s_state = AUTO_CAL_STATE_WARMUP;
            // Slow blink during warmup (1 Hz)
            if (now - lastLedToggle >= 500) {
                ledToggle();
                lastLedToggle = now;
            }
        } else {
            s_state = AUTO_CAL_STATE_SAMPLING;
            // Fast blink during sampling (4 Hz)
            if (now - lastLedToggle >= 125) {
                ledToggle();
                lastLedToggle = now;
            }
        }
        
        // Take sample at interval
        if (now - lastSampleTime >= CAL_SAMPLE_INTERVAL_MS) {
            lastSampleTime = now;
            
            uint32_t freq = moistureProbe_measureFrequency(probeIndex, PROBE_MEASUREMENT_MS);
            s_progress.currentFreq = freq;
            
            // Validate frequency
            ProbeStatus status = moistureProbe_validateFrequency(freq);
            if (status != PROBE_OK) {
                DEBUG_PRINTF("AutoCal: Invalid frequency %lu Hz (status=%d)\n", freq, status);
                // Continue sampling - might be transient
            } else {
                addSample(freq);
            }
            
            // Check stability after warmup and window is full
            if (s_windowFull && elapsed >= CAL_MIN_DURATION_MS) {
                float mean = computeMean();
                float stdDev = computeStdDev(mean);
                float relativeStd = (mean > 0) ? (stdDev / mean) : 1.0f;
                
                s_progress.stability = relativeStd;
                
                // Track best result
                if (relativeStd < bestStability) {
                    bestStability = relativeStd;
                    bestFreq = computeTrimmedMean();
                }
                
                DEBUG_PRINTF("AutoCal: freq=%lu, mean=%.0f, std=%.1f, rel=%.4f\n",
                             freq, mean, stdDev, relativeStd);
                
                // Check if stable enough
                if (relativeStd < CAL_STABILITY_THRESHOLD) {
                    s_progress.isStable = true;
                    s_state = AUTO_CAL_STATE_STABLE;
                    DEBUG_PRINTLN("AutoCal: Stability reached!");
                    break;
                }
            }
        }
        
        // Small delay to prevent tight loop
        delay(10);
    }
    
    // Power off probes
    moistureProbe_powerOff();
    
    // Determine result
    uint32_t finalFreq = 0;
    AutoCalResult res;
    
    if (s_abortRequested) {
        res = AUTO_CAL_ABORTED;
        ledOff();
        DEBUG_PRINTLN("AutoCal: Aborted");
    } else if (s_progress.isStable) {
        // Use trimmed mean of stable window
        finalFreq = computeTrimmedMean();
        res = AUTO_CAL_OK;
        
        // Save calibration
        moistureCal_setAir(probeIndex, finalFreq);
        
        // LED: solid on to indicate success
        ledOn();
        s_state = AUTO_CAL_STATE_COMPLETE;
        DEBUG_PRINTF("AutoCal: Success! f_air = %lu Hz\n", finalFreq);
    } else {
        // Timeout - use best result we found
        finalFreq = bestFreq;
        res = AUTO_CAL_TIMEOUT;
        
        if (finalFreq > 0) {
            // Save marginal calibration
            moistureCal_setAir(probeIndex, finalFreq);
            DEBUG_PRINTF("AutoCal: Timeout, using best f_air = %lu Hz (stability=%.4f)\n",
                         finalFreq, bestStability);
        }
        
        // LED: slow blink to indicate marginal result
        // (caller should handle this)
        s_state = AUTO_CAL_STATE_COMPLETE;
    }
    
    if (result) *result = res;
    return finalFreq;
}

/**
 * @brief Run auto-calibration for all probes that need it
 */
uint8_t autoCal_runAll(void) {
    uint8_t successCount = 0;
    
    for (uint8_t i = 0; i < NUM_MOISTURE_PROBES; i++) {
        if (moistureCal_needsAirCal(i)) {
            DEBUG_PRINTF("AutoCal: Probe %d needs calibration\n", i);
            
            AutoCalResult result;
            uint32_t freq = autoCal_runSingle(i, &result);
            
            if (result == AUTO_CAL_OK || (result == AUTO_CAL_TIMEOUT && freq > 0)) {
                successCount++;
            }
            
            if (s_abortRequested) {
                break;
            }
            
            // Brief pause between probes
            delay(500);
        }
    }
    
    // Final LED state: solid if all successful
    if (successCount == NUM_MOISTURE_PROBES) {
        ledOn();
    } else {
        // Blink pattern for partial success
        for (int i = 0; i < 5; i++) {
            ledToggle();
            delay(200);
        }
        ledOn();
    }
    
    return successCount;
}

/**
 * @brief Get current calibration progress
 */
void autoCal_getProgress(AutoCalProgress* progress) {
    if (progress) {
        progress->state = s_state;
        progress->probeIndex = s_progress.probeIndex;
        progress->elapsedMs = s_progress.elapsedMs;
        progress->currentFreq = s_progress.currentFreq;
        progress->stability = s_progress.stability;
        progress->isStable = s_progress.isStable;
    }
}

/**
 * @brief Abort ongoing calibration
 */
void autoCal_abort(void) {
    s_abortRequested = true;
}

/**
 * @brief Check if calibration is currently running
 */
bool autoCal_isRunning(void) {
    return (s_state != AUTO_CAL_STATE_IDLE && 
            s_state != AUTO_CAL_STATE_COMPLETE && 
            s_state != AUTO_CAL_STATE_ERROR);
}
