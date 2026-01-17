/**
 * @file battery_adc.h
 * @brief Battery voltage ADC driver for Soil Moisture Sensor
 */

#ifndef BATTERY_ADC_H
#define BATTERY_ADC_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the battery ADC
 * @return true if successful
 */
bool battery_adc_init(void);

/**
 * @brief Read battery voltage in millivolts
 * @return Battery voltage in mV, or 0 on error
 */
uint16_t battery_adc_read_mv(void);

/**
 * @brief Deinitialize the battery ADC (for power saving)
 */
void battery_adc_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* BATTERY_ADC_H */
