/**
 * @file moisture_probe.h
 * @brief Oscillator-based Soil Moisture Probe Interface
 * 
 * Provides functions for measuring soil moisture using relaxation oscillator
 * frequency shift method. Each probe contains a 74LVC1G17 Schmitt trigger
 * oscillator where frequency varies with soil capacitance.
 * 
 * Probes are installed at multiple depths (1, 3, 5, 7 feet).
 * Frequency is measured using nRF52832 Timer + GPIOTE + PPI.
 */

#ifndef MOISTURE_PROBE_H
#define MOISTURE_PROBE_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

// MAX_PROBES is defined in config.h

// Probe status codes
typedef enum {
    PROBE_OK = 0,
    PROBE_DISCONNECTED,     // Frequency out of valid range (too high)
    PROBE_SHORTED,          // Frequency out of valid range (too low)
    PROBE_NOT_CALIBRATED,   // Missing calibration data
    PROBE_ERROR             // General error
} ProbeStatus;

// Single probe reading
typedef struct {
    uint8_t     probeIndex;     // 0-3
    uint32_t    frequency;      // Measured frequency in Hz
    uint8_t     moisturePercent;// Calculated moisture (0-100), 255 if invalid
    ProbeStatus status;         // Probe status
} ProbeReading;

// All probes reading
typedef struct {
    ProbeReading probes[MAX_PROBES];
    uint8_t      numProbes;     // Number of probes read
    uint32_t     timestamp;     // millis() when reading was taken
} MoistureReading;

/**
 * @brief Initialize moisture probe hardware
 * 
 * Sets up GPIO pins, timers, and PPI for frequency measurement.
 * Must be called before any other probe functions.
 */
void moistureProbe_init(void);

/**
 * @brief Power on all probes
 * 
 * Activates the P-FET high-side switch to power probe oscillators.
 * Call this before taking measurements.
 */
void moistureProbe_powerOn(void);

/**
 * @brief Power off all probes
 * 
 * Deactivates probe power to save energy.
 * Call this after measurements are complete.
 */
void moistureProbe_powerOff(void);

/**
 * @brief Check if probes are powered
 * @return true if probe power is on
 */
bool moistureProbe_isPowered(void);

/**
 * @brief Measure frequency from a single probe
 * 
 * @param probeIndex Probe number (0-3)
 * @param measurementMs Duration to count edges (longer = more accurate)
 * @return Frequency in Hz, or 0 if error
 */
uint32_t moistureProbe_measureFrequency(uint8_t probeIndex, uint32_t measurementMs);

/**
 * @brief Read a single probe with status
 * 
 * Powers on probes if needed, measures frequency, calculates moisture %.
 * 
 * @param probeIndex Probe number (0-3)
 * @param reading Output: probe reading with frequency, moisture %, status
 * @return true if measurement successful
 */
bool moistureProbe_readSingle(uint8_t probeIndex, ProbeReading* reading);

/**
 * @brief Read all probes sequentially
 * 
 * Powers on probes, reads each one, calculates moisture %, powers off.
 * 
 * @param reading Output: all probe readings
 * @return Number of probes successfully read
 */
uint8_t moistureProbe_readAll(MoistureReading* reading);

/**
 * @brief Convert frequency to moisture percentage
 * 
 * Uses calibration data (f_dry, f_wet) to calculate moisture.
 * 
 * @param probeIndex Probe number (0-3)
 * @param frequency Measured frequency in Hz
 * @return Moisture percentage (0-100), or 255 if not calibrated
 */
uint8_t moistureProbe_frequencyToPercent(uint8_t probeIndex, uint32_t frequency);

/**
 * @brief Check if a frequency is within valid range
 * 
 * @param frequency Frequency in Hz
 * @return PROBE_OK if valid, PROBE_DISCONNECTED or PROBE_SHORTED if not
 */
ProbeStatus moistureProbe_validateFrequency(uint32_t frequency);

/**
 * @brief Get the GPIO pin for a probe
 * 
 * @param probeIndex Probe number (0-3)
 * @return GPIO pin number, or 0xFF if invalid index
 */
uint8_t moistureProbe_getPin(uint8_t probeIndex);

#endif // MOISTURE_PROBE_H
