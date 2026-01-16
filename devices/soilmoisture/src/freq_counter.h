/**
 * @file freq_counter.h
 * @brief Frequency counter for soil moisture probe oscillators
 * 
 * Uses TIMER in counter mode with GPIOTE to count oscillator pulses.
 * Each probe has a relaxation oscillator (74LVC1G17 Schmitt trigger).
 * Frequency varies with soil capacitance: dry = high freq, wet = low freq.
 */

#ifndef FREQ_COUNTER_H
#define FREQ_COUNTER_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize frequency counter hardware
 */
bool freq_counter_init(void);

/**
 * @brief Measure frequency on a probe pin
 * @param probe_index Probe index (0-3)
 * @param measurement_ms Measurement window in milliseconds
 * @return Frequency in Hz, or 0 on error
 */
uint32_t freq_counter_measure(uint8_t probe_index, uint32_t measurement_ms);

/**
 * @brief Power on probe oscillators
 */
void freq_counter_power_on(void);

/**
 * @brief Power off probe oscillators
 */
void freq_counter_power_off(void);

/**
 * @brief Check if frequency is within valid range
 */
bool freq_counter_is_valid(uint32_t freq_hz);

#endif /* FREQ_COUNTER_H */
