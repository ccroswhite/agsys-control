/**
 * @file agsys_ota.h
 * @brief Over-The-Air Firmware Update Module
 * 
 * Provides application-controlled OTA updates with automatic backup
 * and rollback support. Works with both BLE and LoRa transports.
 * 
 * OTA Flow:
 *   1. agsys_ota_start() - Backup current firmware, prepare for update
 *   2. agsys_ota_write_chunk() - Receive firmware chunks (staged in external flash)
 *   3. agsys_ota_finish() - Verify, apply to internal flash, reboot
 *   4. After reboot: agsys_ota_confirm() - Mark firmware as good
 * 
 * If confirm() not called within timeout, next boot triggers rollback.
 */

#ifndef AGSYS_OTA_H
#define AGSYS_OTA_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "FreeRTOS.h"
#include "task.h"
#include "agsys_flash.h"
#include "agsys_flash_backup.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * CONFIGURATION
 * ========================================================================== */

/* Staging area in external flash (after backup slots) */
#define AGSYS_OTA_STAGING_ADDR      0x100000    /* 1MB offset (in log region) */
#define AGSYS_OTA_STAGING_SIZE      0x080000    /* 512KB max firmware */

/* Chunk size for transfers */
#define AGSYS_OTA_CHUNK_SIZE        256

/* Confirmation timeout after reboot */
#define AGSYS_OTA_CONFIRM_TIMEOUT_MS    60000   /* 60 seconds */

/* ==========================================================================
 * OTA STATUS
 * ========================================================================== */

typedef enum {
    AGSYS_OTA_STATUS_IDLE = 0,          /**< No OTA in progress */
    AGSYS_OTA_STATUS_BACKUP_IN_PROGRESS,/**< Backing up current firmware */
    AGSYS_OTA_STATUS_RECEIVING,         /**< Receiving firmware chunks */
    AGSYS_OTA_STATUS_VERIFYING,         /**< Verifying received firmware */
    AGSYS_OTA_STATUS_APPLYING,          /**< Writing to internal flash */
    AGSYS_OTA_STATUS_PENDING_REBOOT,    /**< Ready to reboot */
    AGSYS_OTA_STATUS_PENDING_CONFIRM,   /**< Awaiting confirmation after reboot */
    AGSYS_OTA_STATUS_ERROR              /**< Error occurred */
} agsys_ota_status_t;

typedef enum {
    AGSYS_OTA_ERR_NONE = 0,
    AGSYS_OTA_ERR_ALREADY_IN_PROGRESS,
    AGSYS_OTA_ERR_BACKUP_FAILED,
    AGSYS_OTA_ERR_FLASH_ERASE,
    AGSYS_OTA_ERR_FLASH_WRITE,
    AGSYS_OTA_ERR_INVALID_CHUNK,
    AGSYS_OTA_ERR_CRC_MISMATCH,
    AGSYS_OTA_ERR_SIZE_MISMATCH,
    AGSYS_OTA_ERR_SIGNATURE_INVALID,
    AGSYS_OTA_ERR_INTERNAL_FLASH,
    AGSYS_OTA_ERR_NOT_STARTED,
    AGSYS_OTA_ERR_TIMEOUT
} agsys_ota_error_t;

/* ==========================================================================
 * OTA CONTEXT
 * ========================================================================== */

typedef struct {
    agsys_flash_ctx_t   *flash;         /**< External flash context */
    agsys_backup_ctx_t  *backup;        /**< Backup context */
    
    agsys_ota_status_t  status;         /**< Current OTA status */
    agsys_ota_error_t   last_error;     /**< Last error code */
    
    /* Update metadata */
    uint32_t    expected_size;          /**< Expected firmware size */
    uint32_t    expected_crc;           /**< Expected CRC32 */
    uint8_t     expected_version[4];    /**< Expected version */
    
    /* Progress tracking */
    uint32_t    bytes_received;         /**< Bytes received so far */
    uint32_t    chunks_received;        /**< Chunks received */
    uint32_t    staging_offset;         /**< Current write offset in staging */
    
    /* Timing */
    uint32_t    start_time_ms;          /**< When OTA started */
    
    bool        initialized;
} agsys_ota_ctx_t;

/* ==========================================================================
 * PROGRESS CALLBACK
 * ========================================================================== */

/**
 * @brief OTA progress callback
 * @param status Current status
 * @param progress Percentage complete (0-100)
 * @param user_data User-provided context
 */
typedef void (*agsys_ota_progress_cb_t)(agsys_ota_status_t status, 
                                         uint8_t progress,
                                         void *user_data);

/**
 * @brief OTA completion callback (called before reboot)
 * 
 * Use this to send ACK to BLE/LoRa before device reboots.
 * 
 * @param success true if OTA succeeded, false if failed
 * @param error Error code if failed
 * @param user_data User-provided context
 */
typedef void (*agsys_ota_complete_cb_t)(bool success,
                                         agsys_ota_error_t error,
                                         void *user_data);

/* ==========================================================================
 * API FUNCTIONS
 * ========================================================================== */

/**
 * @brief Initialize OTA module
 * @param ctx OTA context
 * @param flash Flash context (must be initialized)
 * @param backup Backup context (must be initialized)
 * @return true on success
 */
bool agsys_ota_init(agsys_ota_ctx_t *ctx, 
                     agsys_flash_ctx_t *flash,
                     agsys_backup_ctx_t *backup);

/**
 * @brief Start OTA update session
 * 
 * Backs up current firmware and prepares staging area.
 * 
 * @param ctx OTA context
 * @param fw_size Expected firmware size in bytes
 * @param fw_crc Expected CRC32 of firmware
 * @param major Expected version major
 * @param minor Expected version minor
 * @param patch Expected version patch
 * @return AGSYS_OTA_ERR_NONE on success
 */
agsys_ota_error_t agsys_ota_start(agsys_ota_ctx_t *ctx,
                                   uint32_t fw_size,
                                   uint32_t fw_crc,
                                   uint8_t major,
                                   uint8_t minor,
                                   uint8_t patch);

/**
 * @brief Write a firmware chunk
 * 
 * @param ctx OTA context
 * @param offset Offset within firmware image
 * @param data Chunk data
 * @param len Chunk length
 * @return AGSYS_OTA_ERR_NONE on success
 */
agsys_ota_error_t agsys_ota_write_chunk(agsys_ota_ctx_t *ctx,
                                         uint32_t offset,
                                         const uint8_t *data,
                                         size_t len);

/**
 * @brief Finish OTA update
 * 
 * Verifies received firmware, writes to internal flash, prepares for reboot.
 * 
 * @param ctx OTA context
 * @return AGSYS_OTA_ERR_NONE on success
 */
agsys_ota_error_t agsys_ota_finish(agsys_ota_ctx_t *ctx);

/**
 * @brief Abort OTA update
 * 
 * Cancels in-progress update and cleans up.
 * 
 * @param ctx OTA context
 */
void agsys_ota_abort(agsys_ota_ctx_t *ctx);

/**
 * @brief Reboot to apply update
 * 
 * Call after agsys_ota_finish() returns success.
 * Does not return.
 */
void agsys_ota_reboot(void);

/**
 * @brief Confirm firmware is working
 * 
 * Must be called within CONFIRM_TIMEOUT_MS after reboot.
 * If not called, next boot will trigger rollback.
 * 
 * @param ctx OTA context
 */
void agsys_ota_confirm(agsys_ota_ctx_t *ctx);

/**
 * @brief Check if confirmation is pending
 * @param ctx OTA context
 * @return true if firmware needs confirmation
 */
bool agsys_ota_is_confirm_pending(agsys_ota_ctx_t *ctx);

/**
 * @brief Get current OTA status
 * @param ctx OTA context
 * @return Current status
 */
agsys_ota_status_t agsys_ota_get_status(agsys_ota_ctx_t *ctx);

/**
 * @brief Get last error
 * @param ctx OTA context
 * @return Last error code
 */
agsys_ota_error_t agsys_ota_get_error(agsys_ota_ctx_t *ctx);

/**
 * @brief Get progress percentage
 * @param ctx OTA context
 * @return Progress 0-100
 */
uint8_t agsys_ota_get_progress(agsys_ota_ctx_t *ctx);

/**
 * @brief Set progress callback
 * @param ctx OTA context
 * @param callback Callback function
 * @param user_data User context passed to callback
 */
void agsys_ota_set_progress_callback(agsys_ota_ctx_t *ctx,
                                      agsys_ota_progress_cb_t callback,
                                      void *user_data);

/**
 * @brief Set completion callback (called before reboot)
 * 
 * The callback should send ACK to BLE/LoRa to notify sender
 * that update completed successfully before device reboots.
 * 
 * @param ctx OTA context
 * @param callback Callback function
 * @param user_data User context passed to callback
 */
void agsys_ota_set_complete_callback(agsys_ota_ctx_t *ctx,
                                      agsys_ota_complete_cb_t callback,
                                      void *user_data);

/**
 * @brief Register a task to be suspended during OTA
 * 
 * Call this during init for each task that should be suspended
 * during OTA (ADC, display, sensor tasks, etc.)
 * 
 * @param task Task handle to register
 */
void agsys_ota_register_task(TaskHandle_t task);

/**
 * @brief Suspend other tasks during OTA
 * 
 * Call this after agsys_ota_start() to suspend ADC, display,
 * and other non-essential tasks during firmware update.
 */
void agsys_ota_suspend_tasks(void);

/**
 * @brief Resume tasks after OTA abort
 * 
 * Call this if OTA is aborted to resume normal operation.
 */
void agsys_ota_resume_tasks(void);

#ifdef __cplusplus
}
#endif

#endif /* AGSYS_OTA_H */
