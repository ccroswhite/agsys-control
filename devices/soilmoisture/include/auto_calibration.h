/**
 * @file auto_calibration.h
 * @brief Auto-Calibration Module for First Boot f_air Calibration
 * 
 * Implements adaptive calibration algorithm that:
 * - Runs on first power-up when no calibration data exists
 * - Samples frequency until readings stabilize
 * - Uses trimmed mean for robust averaging
 * - Provides LED feedback during calibration
 */

#ifndef AUTO_CALIBRATION_H
#define AUTO_CALIBRATION_H

#include <stdint.h>
#include <stdbool.h>

// Calibration result codes
typedef enum {
    AUTO_CAL_OK = 0,
    AUTO_CAL_TIMEOUT,       // Max duration reached without stability
    AUTO_CAL_PROBE_ERROR,   // Probe disconnected or shorted
    AUTO_CAL_ABORTED        // Calibration aborted
} AutoCalResult;

// Calibration state (for progress reporting)
typedef enum {
    AUTO_CAL_STATE_IDLE = 0,
    AUTO_CAL_STATE_WARMUP,      // Initial warmup period
    AUTO_CAL_STATE_SAMPLING,    // Collecting samples
    AUTO_CAL_STATE_STABLE,      // Readings stable, finalizing
    AUTO_CAL_STATE_COMPLETE,    // Calibration complete
    AUTO_CAL_STATE_ERROR        // Error occurred
} AutoCalState;

// Calibration progress info
typedef struct {
    AutoCalState state;
    uint8_t      probeIndex;
    uint32_t     elapsedMs;
    uint32_t     currentFreq;
    float        stability;     // Current relative std dev (lower = more stable)
    bool         isStable;
} AutoCalProgress;

/**
 * @brief Initialize auto-calibration module
 */
void autoCal_init(void);

/**
 * @brief Check if any probe needs f_air calibration
 * @return true if at least one probe needs calibration
 */
bool autoCal_needed(void);

/**
 * @brief Run auto-calibration for a single probe
 * 
 * Blocks until calibration completes or times out.
 * Updates LED during calibration.
 * 
 * @param probeIndex Probe to calibrate (0-3)
 * @param result Output: calibration result
 * @return Calibrated f_air frequency, or 0 if failed
 */
uint32_t autoCal_runSingle(uint8_t probeIndex, AutoCalResult* result);

/**
 * @brief Run auto-calibration for all probes that need it
 * 
 * Calibrates each probe sequentially.
 * 
 * @return Number of probes successfully calibrated
 */
uint8_t autoCal_runAll(void);

/**
 * @brief Get current calibration progress
 * 
 * Can be called during calibration to get status.
 * 
 * @param progress Output: current progress info
 */
void autoCal_getProgress(AutoCalProgress* progress);

/**
 * @brief Abort ongoing calibration
 */
void autoCal_abort(void);

/**
 * @brief Check if calibration is currently running
 * @return true if calibration in progress
 */
bool autoCal_isRunning(void);

#endif // AUTO_CALIBRATION_H
