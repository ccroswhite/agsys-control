/**
 * @file agsys_ble_auth.h
 * @brief BLE PIN Authentication for AgSys devices
 * 
 * Provides 6-digit PIN authentication with:
 * - 3 max attempts before lockout
 * - 5-minute lockout period
 * - 5-minute session timeout
 * - PIN stored in FRAM
 */

#ifndef AGSYS_BLE_AUTH_H
#define AGSYS_BLE_AUTH_H

#include <stdint.h>
#include <stdbool.h>
#include "agsys_fram.h"

/* ==========================================================================
 * CONFIGURATION
 * ========================================================================== */

#define AGSYS_PIN_LENGTH            6
#define AGSYS_PIN_MAX_ATTEMPTS      3
#define AGSYS_PIN_LOCKOUT_MS        300000      /* 5 minutes */
#define AGSYS_AUTH_TIMEOUT_MS       300000      /* 5 minutes */
#define AGSYS_DEFAULT_PIN           "123456"

/* ==========================================================================
 * AUTH STATUS CODES
 * ========================================================================== */

#define AGSYS_AUTH_NOT_AUTHENTICATED    0x00
#define AGSYS_AUTH_AUTHENTICATED        0x01
#define AGSYS_AUTH_FAILED               0x02
#define AGSYS_AUTH_LOCKED_OUT           0x03
#define AGSYS_AUTH_PIN_CHANGED          0x04

/* ==========================================================================
 * TYPES
 * ========================================================================== */

/**
 * @brief PIN authentication context
 */
typedef struct {
    char        stored_pin[AGSYS_PIN_LENGTH + 1];   /* Current PIN */
    bool        authenticated;                       /* Session authenticated */
    uint32_t    auth_time_ms;                       /* Time of authentication */
    uint8_t     failed_attempts;                    /* Failed attempt count */
    uint32_t    lockout_start_ms;                   /* Lockout start time */
    uint16_t    fram_pin_addr;                      /* FRAM address for PIN storage */
    bool        initialized;
} agsys_ble_auth_ctx_t;

/**
 * @brief Authentication callback
 */
typedef void (*agsys_ble_auth_callback_t)(bool authenticated);

/* ==========================================================================
 * INITIALIZATION
 * ========================================================================== */

/**
 * @brief Initialize PIN authentication
 * 
 * Loads PIN from FRAM or sets default if not found.
 * 
 * @param ctx           Auth context
 * @param fram_ctx      FRAM context for PIN storage
 * @param fram_pin_addr FRAM address for PIN storage (6 bytes)
 * @return true on success
 */
bool agsys_ble_auth_init(agsys_ble_auth_ctx_t *ctx, 
                          agsys_fram_ctx_t *fram_ctx,
                          uint16_t fram_pin_addr);

/* ==========================================================================
 * AUTHENTICATION
 * ========================================================================== */

/**
 * @brief Verify PIN and authenticate session
 * 
 * @param ctx       Auth context
 * @param pin       PIN to verify (6 ASCII digits)
 * @param len       PIN length (must be 6)
 * @return Auth status code (AGSYS_AUTH_*)
 */
uint8_t agsys_ble_auth_verify_pin(agsys_ble_auth_ctx_t *ctx, 
                                   const uint8_t *pin, 
                                   uint16_t len);

/**
 * @brief Change PIN (requires authentication)
 * 
 * @param ctx       Auth context
 * @param old_pin   Current PIN (6 digits)
 * @param new_pin   New PIN (6 digits)
 * @return Auth status code (AGSYS_AUTH_PIN_CHANGED on success)
 */
uint8_t agsys_ble_auth_change_pin(agsys_ble_auth_ctx_t *ctx,
                                   const uint8_t *old_pin,
                                   const uint8_t *new_pin);

/**
 * @brief Check if session is authenticated (with timeout check)
 * 
 * @param ctx       Auth context
 * @return true if authenticated and not timed out
 */
bool agsys_ble_auth_is_authenticated(agsys_ble_auth_ctx_t *ctx);

/**
 * @brief Clear authentication (call on disconnect)
 * 
 * @param ctx       Auth context
 */
void agsys_ble_auth_clear(agsys_ble_auth_ctx_t *ctx);

/**
 * @brief Get current auth status code
 * 
 * @param ctx       Auth context
 * @return Auth status code (AGSYS_AUTH_*)
 */
uint8_t agsys_ble_auth_get_status(const agsys_ble_auth_ctx_t *ctx);

/**
 * @brief Check if locked out
 * 
 * @param ctx       Auth context
 * @return true if in lockout period
 */
bool agsys_ble_auth_is_locked_out(agsys_ble_auth_ctx_t *ctx);

/**
 * @brief Get remaining lockout time in seconds
 * 
 * @param ctx       Auth context
 * @return Seconds remaining, 0 if not locked out
 */
uint32_t agsys_ble_auth_lockout_remaining(agsys_ble_auth_ctx_t *ctx);

/* ==========================================================================
 * PIN MANAGEMENT
 * ========================================================================== */

/**
 * @brief Reset PIN to default (factory reset)
 * 
 * @param ctx       Auth context
 */
void agsys_ble_auth_reset_pin(agsys_ble_auth_ctx_t *ctx);

/**
 * @brief Load PIN from FRAM
 * 
 * @param ctx       Auth context
 */
void agsys_ble_auth_load_pin(agsys_ble_auth_ctx_t *ctx);

/**
 * @brief Save PIN to FRAM
 * 
 * @param ctx       Auth context
 */
void agsys_ble_auth_save_pin(agsys_ble_auth_ctx_t *ctx);

#endif /* AGSYS_BLE_AUTH_H */
