/**
 * @file agsys_device.h
 * @brief Common device initialization for AgSys FreeRTOS devices
 * 
 * Provides shared initialization for BLE, FRAM, Flash, Log, and Backup
 * that is identical across all device types.
 */

#ifndef AGSYS_DEVICE_H
#define AGSYS_DEVICE_H

#include <stdint.h>
#include <stdbool.h>
#include "agsys_ble.h"
#include "agsys_ble_auth.h"
#include "agsys_fram.h"
#include "agsys_flash.h"
#include "agsys_flash_log.h"
#include "agsys_flash_backup.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * DEVICE TYPES
 * ========================================================================== */

/* Device types are defined in agsys_lora_protocol.h (from agsys-api).
 * Include agsys_protocol.h for AGSYS_DEVICE_TYPE_* definitions.
 */

/* ==========================================================================
 * DEVICE CONTEXT
 * ========================================================================== */

/**
 * @brief Combined device context containing all shared subsystems
 */
typedef struct {
    /* BLE subsystem */
    agsys_ble_ctx_t      ble_ctx;
    agsys_ble_auth_ctx_t auth_ctx;
    
    /* Storage subsystem */
    agsys_fram_ctx_t     fram_ctx;
    agsys_flash_ctx_t    flash_ctx;
    agsys_log_ctx_t      log_ctx;
    agsys_backup_ctx_t   backup_ctx;
    
    /* Device info */
    uint8_t              device_type;
    uint8_t              device_uid[8];
    
    /* Status flags */
    bool                 initialized;
    bool                 flash_available;
    bool                 log_available;
    bool                 backup_available;
} agsys_device_ctx_t;

/**
 * @brief Device initialization parameters
 */
typedef struct {
    const char *device_name;    /**< BLE device name (e.g., "AgSoil") */
    uint8_t     device_type;    /**< Device type (AGSYS_DEVICE_TYPE_*) */
    uint8_t     fram_cs_pin;    /**< FRAM chip select pin */
    uint8_t     flash_cs_pin;   /**< Flash chip select pin (0 to skip flash init) */
    agsys_ble_evt_handler_t evt_handler;  /**< Optional BLE event handler */
} agsys_device_init_t;

/* ==========================================================================
 * API FUNCTIONS
 * ========================================================================== */

/**
 * @brief Initialize all device subsystems
 * 
 * This function initializes:
 * 1. FRAM driver
 * 2. BLE PIN authentication (loads PIN from FRAM)
 * 3. BLE service with config and data characteristics
 * 4. Flash driver (if flash_cs_pin != 0)
 * 5. Encrypted log storage
 * 6. Encrypted firmware backup
 * 
 * @param ctx       Device context to initialize
 * @param init      Initialization parameters
 * @return true on success, false on failure
 */
bool agsys_device_init(agsys_device_ctx_t *ctx, const agsys_device_init_t *init);

/**
 * @brief Start BLE advertising (for pairing mode)
 * @param ctx Device context
 */
void agsys_device_start_advertising(agsys_device_ctx_t *ctx);

/**
 * @brief Stop BLE advertising
 * @param ctx Device context
 */
void agsys_device_stop_advertising(agsys_device_ctx_t *ctx);

/**
 * @brief Check if BLE session is authenticated
 * @param ctx Device context
 * @return true if authenticated
 */
bool agsys_device_is_authenticated(const agsys_device_ctx_t *ctx);

/**
 * @brief Get device UID (reads from FICR)
 * @param uid Buffer to store 8-byte UID
 */
void agsys_device_get_uid(uint8_t uid[8]);

/* ==========================================================================
 * LOGGING API
 * ========================================================================== */

/**
 * @brief Log a sensor reading (for offline storage)
 * @param ctx Device context
 * @param readings Array of sensor readings
 * @param count Number of readings
 * @param battery_mv Battery voltage in mV
 * @return true on success
 */
bool agsys_device_log_sensor(agsys_device_ctx_t *ctx, const uint16_t *readings,
                              uint8_t count, uint16_t battery_mv);

/**
 * @brief Log a meter reading (for offline storage)
 * @param ctx Device context
 * @param flow_rate_mlpm Flow rate in mL/min
 * @param total_volume_ml Total volume in mL
 * @param alarm_flags Alarm flags
 * @return true on success
 */
bool agsys_device_log_meter(agsys_device_ctx_t *ctx, uint32_t flow_rate_mlpm,
                             uint32_t total_volume_ml, uint8_t alarm_flags);

/**
 * @brief Log a valve event (for offline storage)
 * @param ctx Device context
 * @param valve_id Valve identifier
 * @param event_type Event type (open, close, fault)
 * @param position Valve position (0-100%)
 * @return true on success
 */
bool agsys_device_log_valve(agsys_device_ctx_t *ctx, uint8_t valve_id,
                             uint8_t event_type, uint8_t position);

/**
 * @brief Log an alarm (for offline storage)
 * @param ctx Device context
 * @param alarm_type Alarm type
 * @param severity Severity level
 * @param code Alarm code
 * @param message Short message (max 24 chars)
 * @return true on success
 */
bool agsys_device_log_alarm(agsys_device_ctx_t *ctx, uint8_t alarm_type,
                             uint8_t severity, uint16_t code, const char *message);

/**
 * @brief Get number of unsynced log entries
 * @param ctx Device context
 * @return Number of entries waiting to be synced
 */
uint32_t agsys_device_log_pending_count(agsys_device_ctx_t *ctx);

/**
 * @brief Mark oldest log entry as synced
 * @param ctx Device context
 * @return true on success
 */
bool agsys_device_log_mark_synced(agsys_device_ctx_t *ctx);

/* ==========================================================================
 * FIRMWARE BACKUP API
 * ========================================================================== */

/**
 * @brief Check if firmware validation is pending
 * @param ctx Device context
 * @return true if validation needed
 */
bool agsys_device_backup_validation_pending(agsys_device_ctx_t *ctx);

/**
 * @brief Validate current firmware (call after successful boot)
 * @param ctx Device context
 */
void agsys_device_backup_validate(agsys_device_ctx_t *ctx);

/**
 * @brief Create firmware backup before OTA
 * @param ctx Device context
 * @param fw_size Firmware size in bytes
 * @param major Version major
 * @param minor Version minor
 * @param patch Version patch
 * @return true on success
 */
bool agsys_device_backup_create(agsys_device_ctx_t *ctx, uint32_t fw_size,
                                 uint8_t major, uint8_t minor, uint8_t patch);

/**
 * @brief Check validation timeout (call periodically)
 * @param ctx Device context
 * @return true if rollback was triggered
 */
bool agsys_device_backup_check_timeout(agsys_device_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* AGSYS_DEVICE_H */
