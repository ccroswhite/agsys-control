/**
 * @file sleep_manager.h
 * @brief Deep sleep manager for Soil Moisture Sensor
 * 
 * Manages System ON sleep with RTC wake for ultra-low power operation.
 * Uses RTC2 for wake timing (RTC0 is used by SoftDevice, RTC1 by FreeRTOS).
 */

#ifndef SLEEP_MANAGER_H
#define SLEEP_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize sleep manager
 */
bool sleep_manager_init(void);

/**
 * @brief Enter deep sleep for specified duration
 * @param sleep_ms Sleep duration in milliseconds
 * @return Actual sleep duration (may be less if woken early)
 */
uint32_t sleep_manager_sleep(uint32_t sleep_ms);

/**
 * @brief Check if woken by button press
 */
bool sleep_manager_woken_by_button(void);

/**
 * @brief Clear wake source flags
 */
void sleep_manager_clear_wake_flags(void);

/**
 * @brief Prepare peripherals for sleep (put LoRa, FRAM to sleep)
 */
void sleep_manager_prepare_sleep(void);

/**
 * @brief Restore peripherals after wake
 */
void sleep_manager_restore_wake(void);

#endif /* SLEEP_MANAGER_H */
