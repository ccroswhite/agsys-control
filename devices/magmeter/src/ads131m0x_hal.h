/**
 * @file ads131m0x_hal.h
 * @brief HAL wrapper for ADS131M0x driver on nRF52840 magmeter
 * 
 * Provides magmeter-specific initialization and interrupt handling
 * for the platform-agnostic ADS131M0x driver.
 */

#ifndef ADS131M0X_HAL_H
#define ADS131M0X_HAL_H

/* ADS131M0X_DEVICE_M02 must be defined in Makefile CFLAGS */
#include "ads131m0x.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize ADS131M02 for magmeter application
 * 
 * Sets up SPI, GPIO, and initializes the ADC with the specified configuration.
 * 
 * @param ctx Device context to initialize
 * @param cs_pin Chip select pin
 * @param drdy_pin Data ready pin
 * @param sync_pin Sync/reset pin
 * @param osr Oversampling ratio
 * @param gain_ch0 Gain for channel 0 (electrode signal)
 * @param gain_ch1 Gain for channel 1 (coil current sense)
 * @return true on success
 */
bool ads131m0x_hal_init(ads131m0x_ctx_t *ctx,
                        uint8_t cs_pin,
                        uint8_t drdy_pin,
                        uint8_t sync_pin,
                        ads131m0x_osr_t osr,
                        ads131m0x_gain_t gain_ch0,
                        ads131m0x_gain_t gain_ch1);

/**
 * @brief Set DRDY callback for interrupt-driven sampling
 * 
 * @param ctx Device context
 * @param callback Function called when sample is ready
 * @param user_data User data passed to callback
 */
void ads131m0x_hal_set_drdy_callback(ads131m0x_ctx_t *ctx,
                                      void (*callback)(ads131m0x_sample_t*, void*),
                                      void *user_data);

/**
 * @brief Enable DRDY interrupt
 * 
 * @param ctx Device context
 */
void ads131m0x_hal_enable_drdy_interrupt(ads131m0x_ctx_t *ctx);

/**
 * @brief Disable DRDY interrupt
 * 
 * @param ctx Device context
 */
void ads131m0x_hal_disable_drdy_interrupt(ads131m0x_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* ADS131M0X_HAL_H */
