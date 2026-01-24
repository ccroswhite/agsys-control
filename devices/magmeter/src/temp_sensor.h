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
    
    /* Board temperature (NTC) - near ADC for offset drift compensation */
    bool ntc_valid;
    float board_temp_c;
    uint16_t ntc_adc_raw;
    
    /* Coil temperature (TMP102 @ 0x48) - on coil spool */
    bool tmp102_coil_present;
    float coil_temp_c;
    
    /* Electrode temperature (TMP102 @ 0x49) - near capacitive electrodes */
    bool tmp102_electrode_present;
    float electrode_temp_c;
    
    /* Legacy alias for backward compatibility */
    #define tmp102_present tmp102_coil_present
    #define pipe_temp_c coil_temp_c
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
 * @brief Read coil temperature from TMP102 (address 0x48)
 * @param ctx Temperature sensor context
 * @return Temperature in °C, or NAN if sensor not present
 */
float temp_sensor_read_coil(temp_sensor_ctx_t *ctx);

/**
 * @brief Read electrode temperature from TMP102 (address 0x49)
 * @param ctx Temperature sensor context
 * @return Temperature in °C, or NAN if sensor not present
 */
float temp_sensor_read_electrode(temp_sensor_ctx_t *ctx);

/**
 * @brief Read all temperature sensors
 * @param ctx Temperature sensor context
 * 
 * Updates ctx->board_temp_c, ctx->coil_temp_c, and ctx->electrode_temp_c
 */
void temp_sensor_read_all(temp_sensor_ctx_t *ctx);

/**
 * @brief Check if coil TMP102 is present and responding
 * @param ctx Temperature sensor context
 * @return true if coil TMP102 is detected
 */
bool temp_sensor_coil_present(temp_sensor_ctx_t *ctx);

/**
 * @brief Check if electrode TMP102 is present and responding
 * @param ctx Temperature sensor context
 * @return true if electrode TMP102 is detected
 */
bool temp_sensor_electrode_present(temp_sensor_ctx_t *ctx);

/* Legacy function aliases for backward compatibility */
#define temp_sensor_read_pipe temp_sensor_read_coil
#define temp_sensor_tmp102_present temp_sensor_coil_present

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
