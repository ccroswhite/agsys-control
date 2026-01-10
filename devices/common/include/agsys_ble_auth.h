/**
 * @file agsys_ble_auth.h
 * @brief Shared BLE PIN Authentication for AgSys Devices
 * 
 * Provides a common BLE authentication service that can be used by
 * all AgSys IoT devices (valve controller, water meter, soil moisture).
 * 
 * Features:
 * - 6-digit PIN authentication
 * - Lockout after failed attempts
 * - Session timeout
 * - PIN change capability
 * - FRAM storage for PIN persistence
 */

#ifndef AGSYS_BLE_AUTH_H
#define AGSYS_BLE_AUTH_H

#include <Arduino.h>
#include <bluefruit.h>

// PIN Configuration
#define AGSYS_PIN_LENGTH            6           // 6-digit PIN
#define AGSYS_PIN_MAX_ATTEMPTS      3           // Lock out after 3 failed attempts
#define AGSYS_PIN_LOCKOUT_MS        300000      // 5 minute lockout
#define AGSYS_AUTH_TIMEOUT_MS       300000      // 5 minute session timeout
#define AGSYS_DEFAULT_PIN           "123456"    // Default PIN (as string)

// Authentication status codes
#define AGSYS_AUTH_NOT_AUTHENTICATED    0x00
#define AGSYS_AUTH_AUTHENTICATED        0x01
#define AGSYS_AUTH_FAILED               0x02
#define AGSYS_AUTH_LOCKED_OUT           0x03
#define AGSYS_AUTH_PIN_CHANGED          0x04

// BLE Characteristic UUIDs for authentication
// These use the AgSys base UUID: AGSYS-xxxx-4167-5379-732D-4D6167000000
#define BLE_UUID_AUTH_SERVICE           "AGSYS100-4167-5379-732D-4D6167000000"
#define BLE_UUID_PIN_AUTH               "AGSYS101-4167-5379-732D-4D6167000000"  // Write: PIN, Read: status
#define BLE_UUID_PIN_CHANGE             "AGSYS102-4167-5379-732D-4D6167000000"  // Write: old+new PIN

// Callback type for authentication events
typedef void (*AgsysAuthCallback_t)(bool authenticated);

/**
 * @brief Initialize the BLE authentication service
 * 
 * @param framAddr FRAM address to store PIN (must have 6 bytes available)
 * @param callback Optional callback for auth state changes
 */
void agsys_ble_auth_init(uint16_t framAddr, AgsysAuthCallback_t callback);

/**
 * @brief Add authentication characteristics to an existing BLE service
 * 
 * Call this after your main service's begin() but before advertising.
 */
void agsys_ble_auth_begin(void);

/**
 * @brief Check if current session is authenticated
 * 
 * Also checks for session timeout and updates state accordingly.
 * 
 * @return true if authenticated and session valid
 */
bool agsys_ble_auth_check(void);

/**
 * @brief Clear authentication (e.g., on disconnect)
 */
void agsys_ble_auth_clear(void);

/**
 * @brief Set the device PIN
 * 
 * @param pin 6-character PIN string
 */
void agsys_ble_auth_set_pin(const char* pin);

/**
 * @brief Load PIN from FRAM
 * 
 * Called automatically during init, but can be called manually.
 */
void agsys_ble_auth_load_pin(void);

/**
 * @brief Get current authentication status
 * 
 * @return Status code (AGSYS_AUTH_*)
 */
uint8_t agsys_ble_auth_get_status(void);

/**
 * @brief Reset lockout (for factory reset scenarios)
 */
void agsys_ble_auth_reset_lockout(void);

#endif // AGSYS_BLE_AUTH_H
