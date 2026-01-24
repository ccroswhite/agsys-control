/**
 * @file tmp102.h
 * @brief TMP102 Digital Temperature Sensor Driver
 * 
 * Complete driver for Texas Instruments TMP102 I2C temperature sensor.
 * Supports all TMP102 features including extended mode, alert configuration,
 * and low-power shutdown mode.
 * 
 * Features:
 *   - Temperature reading in Celsius, Fahrenheit, or raw
 *   - 12-bit (normal) or 13-bit (extended) resolution
 *   - Configurable conversion rate (0.25Hz to 8Hz)
 *   - Alert output with configurable thresholds
 *   - Shutdown mode for low-power operation
 *   - One-shot conversion mode
 *   - Platform-agnostic I2C abstraction
 * 
 * @note This driver is platform-agnostic. You must provide I2C read/write
 *       functions via the tmp102_i2c_t interface.
 * 
 * @author AgSys
 * @date 2026
 * @license MIT
 * 
 * Copyright (c) 2026 AgSys
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 */

#ifndef TMP102_H
#define TMP102_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * I2C ADDRESSES
 * ========================================================================== */

/**
 * TMP102 I2C addresses based on ADD0 pin connection:
 *   ADD0 → GND:  0x48
 *   ADD0 → VCC:  0x49
 *   ADD0 → SDA:  0x4A
 *   ADD0 → SCL:  0x4B
 */
#define TMP102_ADDR_GND     0x48    /**< ADD0 connected to GND */
#define TMP102_ADDR_VCC     0x49    /**< ADD0 connected to VCC */
#define TMP102_ADDR_SDA     0x4A    /**< ADD0 connected to SDA */
#define TMP102_ADDR_SCL     0x4B    /**< ADD0 connected to SCL */

/* ==========================================================================
 * REGISTER DEFINITIONS
 * ========================================================================== */

#define TMP102_REG_TEMP     0x00    /**< Temperature register (read-only) */
#define TMP102_REG_CONFIG   0x01    /**< Configuration register */
#define TMP102_REG_TLOW     0x02    /**< Low temperature threshold */
#define TMP102_REG_THIGH    0x03    /**< High temperature threshold */

/* ==========================================================================
 * CONFIGURATION REGISTER BITS
 * ========================================================================== */

/* Byte 1 (MSB) */
#define TMP102_CFG_OS       (1 << 7)    /**< One-shot / Conversion ready */
#define TMP102_CFG_R1       (1 << 6)    /**< Converter resolution bit 1 (read-only) */
#define TMP102_CFG_R0       (1 << 5)    /**< Converter resolution bit 0 (read-only) */
#define TMP102_CFG_F1       (1 << 4)    /**< Fault queue bit 1 */
#define TMP102_CFG_F0       (1 << 3)    /**< Fault queue bit 0 */
#define TMP102_CFG_POL      (1 << 2)    /**< Alert polarity */
#define TMP102_CFG_TM       (1 << 1)    /**< Thermostat mode */
#define TMP102_CFG_SD       (1 << 0)    /**< Shutdown mode */

/* Byte 2 (LSB) */
#define TMP102_CFG_CR1      (1 << 7)    /**< Conversion rate bit 1 */
#define TMP102_CFG_CR0      (1 << 6)    /**< Conversion rate bit 0 */
#define TMP102_CFG_AL       (1 << 5)    /**< Alert status (read-only) */
#define TMP102_CFG_EM       (1 << 4)    /**< Extended mode (13-bit) */

/* ==========================================================================
 * ENUMERATIONS
 * ========================================================================== */

/**
 * @brief Conversion rate settings
 */
typedef enum {
    TMP102_RATE_0_25HZ = 0,     /**< 0.25 Hz (4 second period) */
    TMP102_RATE_1HZ    = 1,     /**< 1 Hz (1 second period) */
    TMP102_RATE_4HZ    = 2,     /**< 4 Hz (250ms period) - default */
    TMP102_RATE_8HZ    = 3      /**< 8 Hz (125ms period) */
} tmp102_rate_t;

/**
 * @brief Fault queue settings (consecutive faults before alert)
 */
typedef enum {
    TMP102_FAULTS_1 = 0,        /**< 1 fault (default) */
    TMP102_FAULTS_2 = 1,        /**< 2 consecutive faults */
    TMP102_FAULTS_4 = 2,        /**< 4 consecutive faults */
    TMP102_FAULTS_6 = 3         /**< 6 consecutive faults */
} tmp102_faults_t;

/**
 * @brief Alert polarity
 */
typedef enum {
    TMP102_ALERT_ACTIVE_LOW  = 0,   /**< Alert pin active low (default) */
    TMP102_ALERT_ACTIVE_HIGH = 1    /**< Alert pin active high */
} tmp102_alert_polarity_t;

/**
 * @brief Thermostat mode
 */
typedef enum {
    TMP102_MODE_COMPARATOR = 0,     /**< Comparator mode (default) */
    TMP102_MODE_INTERRUPT  = 1      /**< Interrupt mode */
} tmp102_thermostat_mode_t;

/* ==========================================================================
 * I2C ABSTRACTION
 * ========================================================================== */

/**
 * @brief I2C read function pointer type
 * 
 * @param addr 7-bit I2C address
 * @param reg Register address to read from
 * @param data Buffer to store read data
 * @param len Number of bytes to read
 * @param user_data User-provided context pointer
 * @return true on success, false on error
 */
typedef bool (*tmp102_i2c_read_fn)(uint8_t addr, uint8_t reg, uint8_t *data, 
                                    uint8_t len, void *user_data);

/**
 * @brief I2C write function pointer type
 * 
 * @param addr 7-bit I2C address
 * @param reg Register address to write to
 * @param data Data to write
 * @param len Number of bytes to write
 * @param user_data User-provided context pointer
 * @return true on success, false on error
 */
typedef bool (*tmp102_i2c_write_fn)(uint8_t addr, uint8_t reg, const uint8_t *data,
                                     uint8_t len, void *user_data);

/**
 * @brief I2C interface structure
 */
typedef struct {
    tmp102_i2c_read_fn  read;       /**< I2C read function */
    tmp102_i2c_write_fn write;      /**< I2C write function */
    void               *user_data;  /**< User context passed to I2C functions */
} tmp102_i2c_t;

/* ==========================================================================
 * DEVICE CONTEXT
 * ========================================================================== */

/**
 * @brief TMP102 device configuration
 */
typedef struct {
    uint8_t                   addr;         /**< I2C address (0x48-0x4B) */
    tmp102_rate_t             rate;         /**< Conversion rate */
    tmp102_faults_t           faults;       /**< Fault queue setting */
    tmp102_alert_polarity_t   alert_pol;    /**< Alert polarity */
    tmp102_thermostat_mode_t  therm_mode;   /**< Thermostat mode */
    bool                      extended_mode; /**< 13-bit extended mode */
    bool                      shutdown;     /**< Start in shutdown mode */
} tmp102_config_t;

/**
 * @brief TMP102 device context
 */
typedef struct {
    tmp102_i2c_t    i2c;            /**< I2C interface */
    uint8_t         addr;           /**< I2C address */
    bool            extended_mode;  /**< Extended mode enabled */
    bool            initialized;    /**< Device initialized flag */
} tmp102_ctx_t;

/* ==========================================================================
 * DEFAULT CONFIGURATION
 * ========================================================================== */

/**
 * @brief Default configuration macro
 * 
 * Usage:
 *   tmp102_config_t config = TMP102_CONFIG_DEFAULT(TMP102_ADDR_GND);
 */
#define TMP102_CONFIG_DEFAULT(address) { \
    .addr = (address), \
    .rate = TMP102_RATE_4HZ, \
    .faults = TMP102_FAULTS_1, \
    .alert_pol = TMP102_ALERT_ACTIVE_LOW, \
    .therm_mode = TMP102_MODE_COMPARATOR, \
    .extended_mode = false, \
    .shutdown = false \
}

/* ==========================================================================
 * INITIALIZATION
 * ========================================================================== */

/**
 * @brief Initialize TMP102 device
 * 
 * @param ctx Device context to initialize
 * @param i2c I2C interface
 * @param config Device configuration
 * @return true on success, false on error
 */
bool tmp102_init(tmp102_ctx_t *ctx, const tmp102_i2c_t *i2c, 
                 const tmp102_config_t *config);

/**
 * @brief Check if TMP102 is present on the I2C bus
 * 
 * @param i2c I2C interface
 * @param addr I2C address to check
 * @return true if device responds, false otherwise
 */
bool tmp102_is_present(const tmp102_i2c_t *i2c, uint8_t addr);

/**
 * @brief Reset device to default configuration
 * 
 * @param ctx Device context
 * @return true on success
 */
bool tmp102_reset(tmp102_ctx_t *ctx);

/* ==========================================================================
 * TEMPERATURE READING
 * ========================================================================== */

/**
 * @brief Read temperature in Celsius
 * 
 * @param ctx Device context
 * @param temp_c Pointer to store temperature (°C)
 * @return true on success, false on error
 */
bool tmp102_read_temp_c(tmp102_ctx_t *ctx, float *temp_c);

/**
 * @brief Read temperature in Fahrenheit
 * 
 * @param ctx Device context
 * @param temp_f Pointer to store temperature (°F)
 * @return true on success, false on error
 */
bool tmp102_read_temp_f(tmp102_ctx_t *ctx, float *temp_f);

/**
 * @brief Read raw temperature value
 * 
 * @param ctx Device context
 * @param raw Pointer to store raw 12/13-bit value (sign-extended to int16_t)
 * @return true on success, false on error
 */
bool tmp102_read_raw(tmp102_ctx_t *ctx, int16_t *raw);

/* ==========================================================================
 * CONFIGURATION
 * ========================================================================== */

/**
 * @brief Set conversion rate
 * 
 * @param ctx Device context
 * @param rate Conversion rate
 * @return true on success
 */
bool tmp102_set_rate(tmp102_ctx_t *ctx, tmp102_rate_t rate);

/**
 * @brief Enable or disable extended mode (13-bit resolution)
 * 
 * Normal mode: 12-bit, -55°C to +128°C
 * Extended mode: 13-bit, -55°C to +150°C
 * 
 * @param ctx Device context
 * @param enable true to enable extended mode
 * @return true on success
 */
bool tmp102_set_extended_mode(tmp102_ctx_t *ctx, bool enable);

/**
 * @brief Enter or exit shutdown mode
 * 
 * In shutdown mode, the device consumes <0.5µA.
 * Use tmp102_one_shot() to trigger a single conversion.
 * 
 * @param ctx Device context
 * @param shutdown true to enter shutdown, false to exit
 * @return true on success
 */
bool tmp102_set_shutdown(tmp102_ctx_t *ctx, bool shutdown);

/**
 * @brief Trigger one-shot conversion (only valid in shutdown mode)
 * 
 * After calling this, wait for conversion to complete:
 *   - 12-bit mode: 26ms typical
 *   - 13-bit mode: 26ms typical
 * 
 * @param ctx Device context
 * @return true on success
 */
bool tmp102_one_shot(tmp102_ctx_t *ctx);

/**
 * @brief Check if conversion is complete (one-shot mode)
 * 
 * @param ctx Device context
 * @param ready Pointer to store ready status
 * @return true on success (check *ready for conversion status)
 */
bool tmp102_conversion_ready(tmp102_ctx_t *ctx, bool *ready);

/* ==========================================================================
 * ALERT CONFIGURATION
 * ========================================================================== */

/**
 * @brief Set alert thresholds
 * 
 * @param ctx Device context
 * @param t_low Low threshold in Celsius
 * @param t_high High threshold in Celsius
 * @return true on success
 */
bool tmp102_set_alert_thresholds(tmp102_ctx_t *ctx, float t_low, float t_high);

/**
 * @brief Get alert thresholds
 * 
 * @param ctx Device context
 * @param t_low Pointer to store low threshold (°C)
 * @param t_high Pointer to store high threshold (°C)
 * @return true on success
 */
bool tmp102_get_alert_thresholds(tmp102_ctx_t *ctx, float *t_low, float *t_high);

/**
 * @brief Set alert polarity
 * 
 * @param ctx Device context
 * @param polarity Alert pin polarity
 * @return true on success
 */
bool tmp102_set_alert_polarity(tmp102_ctx_t *ctx, tmp102_alert_polarity_t polarity);

/**
 * @brief Set thermostat mode
 * 
 * Comparator mode: Alert asserts when temp > T_HIGH, deasserts when temp < T_LOW
 * Interrupt mode: Alert asserts on threshold crossing, cleared by reading temp
 * 
 * @param ctx Device context
 * @param mode Thermostat mode
 * @return true on success
 */
bool tmp102_set_thermostat_mode(tmp102_ctx_t *ctx, tmp102_thermostat_mode_t mode);

/**
 * @brief Set fault queue (consecutive faults before alert)
 * 
 * @param ctx Device context
 * @param faults Fault queue setting
 * @return true on success
 */
bool tmp102_set_fault_queue(tmp102_ctx_t *ctx, tmp102_faults_t faults);

/**
 * @brief Read alert status
 * 
 * @param ctx Device context
 * @param alert Pointer to store alert status (true = alert active)
 * @return true on success
 */
bool tmp102_read_alert_status(tmp102_ctx_t *ctx, bool *alert);

/* ==========================================================================
 * LOW-LEVEL REGISTER ACCESS
 * ========================================================================== */

/**
 * @brief Read configuration register
 * 
 * @param ctx Device context
 * @param config Pointer to store 16-bit config value
 * @return true on success
 */
bool tmp102_read_config(tmp102_ctx_t *ctx, uint16_t *config);

/**
 * @brief Write configuration register
 * 
 * @param ctx Device context
 * @param config 16-bit config value to write
 * @return true on success
 */
bool tmp102_write_config(tmp102_ctx_t *ctx, uint16_t config);

/* ==========================================================================
 * UTILITY FUNCTIONS
 * ========================================================================== */

/**
 * @brief Convert raw value to Celsius
 * 
 * @param raw Raw 12/13-bit value
 * @param extended_mode true if 13-bit mode
 * @return Temperature in Celsius
 */
float tmp102_raw_to_celsius(int16_t raw, bool extended_mode);

/**
 * @brief Convert Celsius to raw value
 * 
 * @param temp_c Temperature in Celsius
 * @param extended_mode true if 13-bit mode
 * @return Raw 12/13-bit value
 */
int16_t tmp102_celsius_to_raw(float temp_c, bool extended_mode);

#ifdef __cplusplus
}
#endif

#endif /* TMP102_H */
