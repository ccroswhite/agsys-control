/**
 * @file temp_sensor.h
 * @brief Temperature Sensor Driver for Magmeter
 * 
 * Supports two temperature sensors:
 *   1. Board temperature - NTC thermistor via ADC
 *   2. Pipe/coil temperature - TMP102 via I2C
 */

#ifndef TEMP_SENSOR_H
#define TEMP_SENSOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * TYPES
 * ========================================================================== */

typedef struct {
    bool initialized;
    
    /* Board temperature (NTC) */
    bool ntc_valid;
    float board_temp_c;
    uint16_t ntc_adc_raw;
    
    /* Pipe/coil temperature (TMP102) */
    bool tmp102_present;
    float pipe_temp_c;
    
    /* Derived values */
    float coil_temp_c;          /* Estimated from resistance if available */
} temp_sensor_ctx_t;

/* ==========================================================================
 * API FUNCTIONS
 * ========================================================================== */

/**
 * @brief Initialize temperature sensors
 * @param ctx Temperature sensor context
 * @return true on success
 */
bool temp_sensor_init(temp_sensor_ctx_t *ctx);

/**
 * @brief Read board temperature from NTC thermistor
 * @param ctx Temperature sensor context
 * @return Temperature in °C, or NAN if error
 */
float temp_sensor_read_board(temp_sensor_ctx_t *ctx);

/**
 * @brief Read pipe/coil temperature from TMP102
 * @param ctx Temperature sensor context
 * @return Temperature in °C, or NAN if sensor not present
 */
float temp_sensor_read_pipe(temp_sensor_ctx_t *ctx);

/**
 * @brief Read all temperature sensors
 * @param ctx Temperature sensor context
 * 
 * Updates ctx->board_temp_c and ctx->pipe_temp_c
 */
void temp_sensor_read_all(temp_sensor_ctx_t *ctx);

/**
 * @brief Check if TMP102 is present and responding
 * @param ctx Temperature sensor context
 * @return true if TMP102 is detected
 */
bool temp_sensor_tmp102_present(temp_sensor_ctx_t *ctx);

/**
 * @brief Estimate coil temperature from measured resistance
 * 
 * Uses copper tempco (+0.393%/°C) to estimate temperature
 * from the ratio of measured to calibrated resistance.
 * 
 * @param r_measured Measured coil resistance in milliohms
 * @param r_cal Calibrated resistance at 25°C in milliohms
 * @return Estimated temperature in °C
 */
float temp_sensor_estimate_coil_temp(uint32_t r_measured, uint32_t r_cal);

#ifdef __cplusplus
}
#endif

#endif /* TEMP_SENSOR_H */
