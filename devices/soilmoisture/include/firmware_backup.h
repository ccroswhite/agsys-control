/**
 * @file firmware_backup.h
 * @brief Firmware backup and rollback using external W25Q16 flash
 * 
 * Stores encrypted backup of current firmware in external SPI flash.
 * Supports automatic rollback if new firmware fails validation.
 * 
 * Flash Layout (2MB W25Q16):
 *   0x000000 - 0x0000FF: Backup header (256 bytes)
 *   0x000100 - 0x07FFFF: Backup slot A (~512KB)
 *   0x080000 - 0x0FFFFF: Backup slot B (~512KB)
 *   0x100000 - 0x1FFFFF: Reserved for future use (1MB)
 * 
 * Rollback mechanism:
 *   1. Before OTA: Current firmware backed up to inactive slot
 *   2. After OTA: New firmware must call fw_backup_validate() within 60s
 *   3. If validation timeout: Bootloader restores from backup slot
 */

#ifndef FIRMWARE_BACKUP_H
#define FIRMWARE_BACKUP_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Flash layout
#define FW_BACKUP_HEADER_ADDR       0x000000
#define FW_BACKUP_HEADER_SIZE       256
#define FW_BACKUP_SLOT_A_ADDR       0x000100
#define FW_BACKUP_SLOT_B_ADDR       0x080000
#define FW_BACKUP_SLOT_SIZE         0x07FF00    // ~512KB per slot
#define FW_BACKUP_RESERVED_ADDR     0x100000

// Header magic
#define FW_BACKUP_MAGIC             0x46574241  // "FWBA"

// Backup status
#define FW_BACKUP_STATUS_EMPTY      0x00
#define FW_BACKUP_STATUS_VALID      0x01
#define FW_BACKUP_STATUS_PENDING    0x02    // Awaiting validation
#define FW_BACKUP_STATUS_FAILED     0x03    // Validation failed

// Backup header structure (stored at FW_BACKUP_HEADER_ADDR)
typedef struct {
    uint32_t magic;                 // FW_BACKUP_MAGIC
    uint8_t  version;               // Header version (2 = with expected version)
    uint8_t  activeSlot;            // Currently active slot (0=A, 1=B)
    uint8_t  slotAStatus;           // Status of slot A
    uint8_t  slotBStatus;           // Status of slot B
    uint32_t slotASize;             // Firmware size in slot A
    uint32_t slotBSize;             // Firmware size in slot B
    uint32_t slotACrc;              // CRC32 of slot A firmware
    uint32_t slotBCrc;              // CRC32 of slot B firmware
    uint8_t  slotAVersion[4];       // Version in slot A (major.minor.patch.build)
    uint8_t  slotBVersion[4];       // Version in slot B
    uint8_t  expectedVersion[4];    // Expected version after OTA (for validation)
    uint8_t  failedVersion[4];      // Last version that failed validation
    uint32_t validationStartMs;     // millis() when validation started
    uint8_t  rollbackCount;         // Number of rollbacks performed
    uint8_t  reserved[207];         // Reserved for future use
    uint32_t headerCrc;             // CRC32 of header (excluding this field)
} __attribute__((packed)) FwBackupHeader;

/**
 * @brief Initialize firmware backup system
 * 
 * Initializes external flash and reads backup header.
 * Must be called early in boot, before OTA checks.
 * 
 * @return true on success
 */
bool fw_backup_init(void);

/**
 * @brief Check if rollback is needed
 * 
 * Called early in boot to check if previous firmware failed validation.
 * If validation timeout occurred, this triggers rollback.
 * 
 * @return true if rollback was triggered
 */
bool fw_backup_check_rollback(void);

/**
 * @brief Mark current firmware as validated
 * 
 * Must be called within FW_VALIDATION_TIMEOUT_MS after boot.
 * If not called, next boot will trigger rollback.
 */
void fw_backup_validate(void);

/**
 * @brief Check if validation is pending
 * @return true if firmware needs validation
 */
bool fw_backup_is_validation_pending(void);

/**
 * @brief Start the validation timer
 * 
 * Called after OTA update completes. Marks firmware as pending
 * and starts the validation countdown.
 */
void fw_backup_start_validation_timer(void);

/**
 * @brief Set expected version after OTA
 * 
 * Called before applying OTA update. Stores the version we expect
 * to be running after reboot. If running version doesn't match,
 * rollback is triggered.
 * 
 * @param major Major version number
 * @param minor Minor version number  
 * @param patch Patch version number
 */
void fw_backup_set_expected_version(uint8_t major, uint8_t minor, uint8_t patch);

/**
 * @brief Check if last boot was a rollback
 * @return true if rollback occurred on last boot
 */
bool fw_backup_was_rollback(void);

/**
 * @brief Get the version that failed validation
 * @param major Output: major version
 * @param minor Output: minor version
 * @param patch Output: patch version
 * @return true if a failed version is recorded
 */
bool fw_backup_get_failed_version(uint8_t* major, uint8_t* minor, uint8_t* patch);

/**
 * @brief Check if validation timeout has expired
 * 
 * Should be called periodically (e.g., in loop()).
 * If timeout expires, triggers automatic rollback.
 * 
 * @return true if rollback was triggered
 */
bool fw_backup_check_validation_timeout(void);

/**
 * @brief Backup current firmware before OTA update
 * 
 * Reads current firmware from internal flash and writes encrypted
 * copy to external flash backup slot.
 * 
 * @param fwSize Size of current firmware in bytes
 * @return true on success
 */
bool fw_backup_create(uint32_t fwSize);

/**
 * @brief Restore firmware from backup slot
 * 
 * Reads encrypted backup from external flash, decrypts, and
 * writes to internal flash. Then resets device.
 * 
 * @return Does not return on success (device resets)
 */
bool fw_backup_restore(void);

/**
 * @brief Manually trigger rollback
 * 
 * Forces restoration of backup firmware. Use for testing
 * or manual recovery.
 */
void fw_backup_force_rollback(void);

/**
 * @brief Get backup status information
 * 
 * @param header Output: Copy of backup header
 * @return true if header is valid
 */
bool fw_backup_get_status(FwBackupHeader* header);

/**
 * @brief Erase all backup data
 * 
 * Clears both backup slots and header. Use with caution.
 */
void fw_backup_erase_all(void);

/**
 * @brief Get rollback count
 * @return Number of times rollback has occurred
 */
uint8_t fw_backup_get_rollback_count(void);

#ifdef __cplusplus
}
#endif

#endif // FIRMWARE_BACKUP_H
