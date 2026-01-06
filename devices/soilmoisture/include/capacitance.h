/**
 * @file capacitance.h
 * @brief AC Capacitance Soil Moisture Measurement Interface
 * 
 * Provides functions for measuring soil moisture using a 100kHz AC
 * capacitance method with discrete MOSFET H-bridge drive.
 */

#ifndef CAPACITANCE_H
#define CAPACITANCE_H

#include <stdint.h>

/**
 * @brief Initialize capacitance measurement hardware
 * 
 * Sets up GPIO pins, GPIOTE, Timer, and PPI for hardware-driven
 * 100kHz H-bridge AC generation.
 */
void capacitanceInit();

/**
 * @brief Start H-bridge 100kHz AC drive
 * 
 * Enables power to H-bridge and starts hardware-driven complementary
 * GPIO toggling at 100kHz. Runs until hbridgeStop() is called.
 */
void hbridgeStart();

/**
 * @brief Stop H-bridge AC drive
 * 
 * Stops timer, disables PPI, sets GPIOs low, and powers down H-bridge.
 */
void hbridgeStop();

/**
 * @brief Read envelope detector with averaging
 * 
 * @param durationMs Measurement duration in milliseconds
 * @param numSamples Number of ADC samples to average
 * @return Average ADC value (0-4095)
 */
uint16_t readEnvelopeAverage(uint32_t durationMs, uint32_t numSamples);

/**
 * @brief Perform complete capacitance measurement
 * 
 * Starts H-bridge, waits for settle, takes 1-second averaged reading,
 * then stops H-bridge. This is the main function to call for moisture
 * measurement.
 * 
 * @return Raw ADC value (higher = more moisture)
 */
uint16_t readCapacitance();

/**
 * @brief Convert raw capacitance to moisture percentage
 * 
 * @param raw Raw ADC value from readCapacitance()
 * @return Moisture percentage (0-100)
 */
uint8_t capacitanceToMoisturePercent(uint16_t raw);

/**
 * @brief Check if H-bridge is currently running
 * @return true if AC drive is active
 */
bool isHbridgeRunning();

#endif // CAPACITANCE_H
