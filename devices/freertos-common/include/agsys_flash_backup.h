/**
 * @file agsys_flash_backup.h
 * @brief Encrypted Firmware Backup and Rollback for W25Q16 Flash
 * 
 * Stores encrypted backup of current firmware in external SPI flash.
 * Supports automatic rollback if new firmware fails validation.
 * 
 * Flash Layout (first 1MB):
 *   0x000000 - 0x000FFF: Backup header (4KB sector)
 *   0x001000 - 0x07FFFF: Backup slot A (~508KB)
 *   0x080000 - 0x0FFFFF: Backup slot B (~512KB)
 * 
 * Encryption: AES-128-CTR with device-specific key
 * Key derivation: SHA-256(SECRET_SALT || DEVICE_ID)[0:16]
 * 
 * Rollback mechanism:
 *   1. Before OTA: Current firmware backed up to inactive slot
 *   2. After OTA: New firmware must call validate() within 60s
 *   3. If validation timeout: Bootloader restores from backup slot
 */

#ifndef AGSYS_FLASH_BACKUP_H
#define AGSYS_FLASH_BACKUP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "agsys_flash.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * FLASH LAYOUT
 * ========================================================================== */

#define AGSYS_BACKUP_HEADER_ADDR    0x000000
#define AGSYS_BACKUP_HEADER_SIZE    4096        /* 1 sector */
#define AGSYS_BACKUP_SLOT_A_ADDR    0x001000
#define AGSYS_BACKUP_SLOT_B_ADDR    0x080000
#define AGSYS_BACKUP_SLOT_SIZE      0x07F000    /* ~508KB per slot */

/* Header magic */
#define AGSYS_BACKUP_MAGIC          0x46574241  /* "FWBA" */

/* Validation timeout */
#define AGSYS_BACKUP_VALIDATION_TIMEOUT_MS  60000   /* 60 seconds */

/* ==========================================================================
 * BACKUP STATUS
 * ========================================================================== */

typedef enum {
    AGSYS_BACKUP_STATUS_EMPTY       = 0x00,
    AGSYS_BACKUP_STATUS_VALID       = 0x01,
    AGSYS_BACKUP_STATUS_PENDING     = 0x02,     /* Awaiting validation */
    AGSYS_BACKUP_STATUS_FAILED      = 0x03      /* Validation failed */
} agsys_backup_status_t;

/* ==========================================================================
 * BACKUP HEADER
 * ========================================================================== */

typedef struct {
    uint32_t magic;                 /**< AGSYS_BACKUP_MAGIC */
    uint8_t  version;               /**< Header version */
    uint8_t  active_slot;           /**< Currently active slot (0=A, 1=B) */
    uint8_t  slot_a_status;         /**< Status of slot A */
    uint8_t  slot_b_status;         /**< Status of slot B */
    uint32_t slot_a_size;           /**< Firmware size in slot A */
    uint32_t slot_b_size;           /**< Firmware size in slot B */
    uint32_t slot_a_crc;            /**< CRC32 of slot A firmware */
    uint32_t slot_b_crc;            /**< CRC32 of slot B firmware */
    uint8_t  slot_a_version[4];     /**< Version in slot A (major.minor.patch.build) */
    uint8_t  slot_b_version[4];     /**< Version in slot B */
    uint8_t  expected_version[4];   /**< Expected version after OTA */
    uint8_t  failed_version[4];     /**< Last version that failed validation */
    uint32_t validation_start_ms;   /**< Tick when validation started */
    uint8_t  rollback_count;        /**< Number of rollbacks performed */
    uint8_t  reserved[207];         /**< Reserved for future use */
    uint32_t header_crc;            /**< CRC32 of header */
} __attribute__((packed)) agsys_backup_header_t;

/* ==========================================================================
 * BACKUP CONTEXT
 * ========================================================================== */

typedef struct {
    agsys_flash_ctx_t *flash;       /**< Flash driver context */
    agsys_backup_header_t header;   /**< Cached header */
    uint8_t  key[16];               /**< Encryption key */
    bool     initialized;
    bool     rollback_occurred;     /**< Set if rollback happened this boot */
} agsys_backup_ctx_t;

/* ==========================================================================
 * API FUNCTIONS
 * ========================================================================== */

/**
 * @brief Initialize firmware backup system
 * @param ctx Backup context
 * @param flash Flash driver context (must be initialized)
 * @return true on success
 */
bool agsys_backup_init(agsys_backup_ctx_t *ctx, agsys_flash_ctx_t *flash);

/**
 * @brief Check if rollback is needed
 * 
 * Called early in boot to check if previous firmware failed validation.
 * If validation timeout occurred, this triggers rollback.
 * 
 * @param ctx Backup context
 * @return true if rollback was triggered
 */
bool agsys_backup_check_rollback(agsys_backup_ctx_t *ctx);

/**
 * @brief Mark current firmware as validated
 * 
 * Must be called within VALIDATION_TIMEOUT_MS after boot.
 * If not called, next boot will trigger rollback.
 * 
 * @param ctx Backup context
 */
void agsys_backup_validate(agsys_backup_ctx_t *ctx);

/**
 * @brief Check if validation is pending
 * @param ctx Backup context
 * @return true if firmware needs validation
 */
bool agsys_backup_is_validation_pending(agsys_backup_ctx_t *ctx);

/**
 * @brief Start the validation timer
 * 
 * Called after OTA update completes. Marks firmware as pending
 * and starts the validation countdown.
 * 
 * @param ctx Backup context
 */
void agsys_backup_start_validation_timer(agsys_backup_ctx_t *ctx);

/**
 * @brief Set expected version after OTA
 * @param ctx Backup context
 * @param major Major version number
 * @param minor Minor version number
 * @param patch Patch version number
 */
void agsys_backup_set_expected_version(agsys_backup_ctx_t *ctx,
                                        uint8_t major, uint8_t minor, uint8_t patch);

/**
 * @brief Check if last boot was a rollback
 * @param ctx Backup context
 * @return true if rollback occurred on last boot
 */
bool agsys_backup_was_rollback(agsys_backup_ctx_t *ctx);

/**
 * @brief Get the version that failed validation
 * @param ctx Backup context
 * @param major Output: major version
 * @param minor Output: minor version
 * @param patch Output: patch version
 * @return true if a failed version is recorded
 */
bool agsys_backup_get_failed_version(agsys_backup_ctx_t *ctx,
                                      uint8_t *major, uint8_t *minor, uint8_t *patch);

/**
 * @brief Check if validation timeout has expired
 * 
 * Should be called periodically (e.g., in main loop).
 * If timeout expires, triggers automatic rollback.
 * 
 * @param ctx Backup context
 * @return true if rollback was triggered
 */
bool agsys_backup_check_validation_timeout(agsys_backup_ctx_t *ctx);

/**
 * @brief Backup current firmware before OTA update
 * 
 * Reads current firmware from internal flash and writes encrypted
 * copy to external flash backup slot.
 * 
 * @param ctx Backup context
 * @param fw_size Size of current firmware in bytes
 * @param version Current firmware version (major.minor.patch)
 * @return true on success
 */
bool agsys_backup_create(agsys_backup_ctx_t *ctx, uint32_t fw_size,
                          uint8_t major, uint8_t minor, uint8_t patch);

/**
 * @brief Restore firmware from backup slot
 * 
 * Reads encrypted backup from external flash, decrypts, and
 * writes to internal flash. Then resets device.
 * 
 * @param ctx Backup context
 * @return Does not return on success (device resets)
 */
bool agsys_backup_restore(agsys_backup_ctx_t *ctx);

/**
 * @brief Manually trigger rollback
 * @param ctx Backup context
 */
void agsys_backup_force_rollback(agsys_backup_ctx_t *ctx);

/**
 * @brief Get backup status information
 * @param ctx Backup context
 * @param header Output: Copy of backup header
 * @return true if header is valid
 */
bool agsys_backup_get_status(agsys_backup_ctx_t *ctx, agsys_backup_header_t *header);

/**
 * @brief Erase all backup data
 * @param ctx Backup context
 * @return true on success
 */
bool agsys_backup_erase_all(agsys_backup_ctx_t *ctx);

/**
 * @brief Get rollback count
 * @param ctx Backup context
 * @return Number of times rollback has occurred
 */
uint8_t agsys_backup_get_rollback_count(agsys_backup_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* AGSYS_FLASH_BACKUP_H */
